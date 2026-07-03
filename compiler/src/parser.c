#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
//  ESTRUTURAS SEMÂNTICAS 
// =============================================================================

//  Symbol  — uma entrada na tabela de símbolos.
//  Scope   — um nível de escopo (função ou bloco), formando uma pilha encadeada pelo ponteiro `parent`.

typedef struct Symbol {
    char       *name;
    char       *label;
    TokenType   type;        
    int         initialized;
    int         used;        
    int         line;
    int         column;
    struct Symbol *next;
} Symbol;

struct Scope {
    Symbol *symbols;
    Scope  *parent;
};

// -----------------------------------------------------------------------------
//  Auxiliares de escopo — criação, destruição e busca
// -----------------------------------------------------------------------------

static Scope *criar_escopo(Scope *parent){
    Scope *s = malloc(sizeof(Scope));
    if(!s) return NULL;
    s->symbols = NULL;
    s->parent  = parent;
    return s;
}

static void liberar_escopo(Scope *scope){
    Symbol *sym = scope->symbols;
    while(sym){
        Symbol *next = sym->next;
        free(sym->name);
        free(sym->label);
        free(sym);
        sym = next;
    }
    free(scope);
}

// Busca apenas no escopo corrente (para detectar redeclaração).
static Symbol *buscar_no_escopo_atual(Scope *scope, const char *name){
    for(Symbol *s = scope->symbols; s; s = s->next)
        if(strcmp(s->name, name) == 0) return s;
    return NULL;
}

// Busca subindo a cadeia de escopos (para uso de variável).
static Symbol *buscar_simbolo(Parser *parser, const char *name){
    for(Scope *sc = parser->current_scope; sc; sc = sc->parent){
        Symbol *s = buscar_no_escopo_atual(sc, name);
        if(s) return s;
    }
    return NULL;
}

// -----------------------------------------------------------------------------
//  Auxiliares de escopo — entrar / sair
//  Ao sair, emite aviso para cada variável declarada mas nunca usada.
// -----------------------------------------------------------------------------

static void entrar_escopo(Parser *parser){
    parser->current_scope = criar_escopo(parser->current_scope);
}

static void sair_escopo(Parser *parser){
    if(!parser->current_scope) return;

    for(Symbol *s = parser->current_scope->symbols; s; s = s->next){
        if(!s->used){
            printf(YELLOW
                   "[ AVISO ] Variável '%s' declarada mas nunca usada "
                   "(linha %d, coluna %d)\n"
                   RESET,
                   s->name, s->line, s->column);
        }
    }

    Scope *morto = parser->current_scope;
    parser->current_scope = morto->parent;
    liberar_escopo(morto);
}

// -----------------------------------------------------------------------------
//  Auxiliar semântico: emite erro sem ativar modo de recuperação sintática.
//  Erros semânticos não quebram o fluxo de tokens, então o parser continua.
// -----------------------------------------------------------------------------

static void erro_semantico(Parser *parser, const char *mensagem, int line, int column){
    parser->quantidade_erros++;
    printf("\n");
    printf(RED "[ ERRO SEMÂNTICO #%d ]\n" RESET, parser->quantidade_erros);
    printf(YELLOW "Mensagem:\n" RESET);
    printf("  %s\n\n", mensagem);
    printf(YELLOW "Localização:\n" RESET);
    printf("  Linha: %d\n  Coluna: %d\n\n", line, column);
}

// -----------------------------------------------------------------------------
//  Auxiliar semântico: registra uma variável no escopo corrente.
//  Rejeita redeclaração dentro do mesmo escopo.
// -----------------------------------------------------------------------------

static Symbol *declarar_variavel(Parser *parser,
                                  const char *name, TokenType type,
                                  int initialized,
                                  int line, int column){
    if(buscar_no_escopo_atual(parser->current_scope, name)){
        char msg[256];
        sprintf(msg, "Variável '%s' já declarada neste escopo", name);
        erro_semantico(parser, msg, line, column);
        return NULL;
    }

    // sombreamento — busca nos escopos pai
    for(Scope *sc = parser->current_scope->parent; sc; sc = sc->parent){
        Symbol *s = buscar_no_escopo_atual(sc, name);
        if(s){
            printf(YELLOW
                   "[ AVISO ] Variável '%s' declarada na linha %d sombreia "
                   "declaração anterior (linha %d, coluna %d)\n"
                   RESET,
                   name, line, s->line, s->column);
            break;
        }
    }

    Symbol *s    = malloc(sizeof(Symbol));
    s->name        = strdup(name);
    s->type        = type;
    s->initialized = initialized;
    s->used        = 0;
    s->line        = line;
    s->column      = column;
    s->next        = parser->current_scope->symbols;
    parser->current_scope->symbols = s;

    char label[128];
    snprintf(label, sizeof(label), "%s_%d", name, parser->cg ? parser->cg->var_counter++ : 0);
    s->label = strdup(label);

    // Reserva espaço em .data para esta variável (1 palavra por escalar)
    if (parser->cg) {
        codegen_registrar_global(parser->cg, label, 1);
    }

    return s;
}

// =============================================================================
//  Auxiliar: Calcula o tipo resultante de uma expressão
//  Usado internamente para validar atribuições e devolver tipos
// =============================================================================

// Para operadores binários, retorna o tipo resultante
static TokenType operador_binario_tipo(TokenType op, TokenType left_type, TokenType right_type){
    // Relacionais e lógicos sempre produzem int
    if(op == TOKEN_AND || op == TOKEN_OR  ||
       op == TOKEN_EQ  || op == TOKEN_NEQ ||
       op == TOKEN_LT  || op == TOKEN_GT  ||
       op == TOKEN_LTE || op == TOKEN_GTE)
        return TOKEN_INT;

    if(left_type == TOKEN_ERROR || right_type == TOKEN_ERROR) 
        return TOKEN_ERROR;

    // Qualquer operação com float produz float
    if(left_type == TOKEN_FLOAT || right_type == TOKEN_FLOAT) 
        return TOKEN_FLOAT;
    // int prevalece sobre char
    if(left_type == TOKEN_INT || right_type == TOKEN_INT) 
        return TOKEN_INT;
    return TOKEN_CHAR;
}

// Para operadores unários, retorna o tipo resultante
static TokenType operador_unario_tipo(TokenType op, TokenType operand_type){
    if(op == TOKEN_NOT) 
        return TOKEN_INT;
    return operand_type;
}

// // Converte token de literal para tipo
// static TokenType literal_para_tipo(TokenType token_type){
//     if(token_type == TOKEN_FLOAT_LITERAL) 
//         return TOKEN_FLOAT;
//     return token_type;
// }

// Valida compatibilidade de tipos numa atribuição (critério "types")
static void validar_tipos(Parser *parser,
                           const char *name,
                           TokenType dest, TokenType expr,
                           int line, int column){
    if(expr == TOKEN_ERROR) return;

    // void nunca recebe valor
    if(dest == TOKEN_VOID){
        char msg[256];
        sprintf(msg, "Atribuição inválida: variável '%s' é do tipo void", name);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // Expressão void não pode ser atribuída
    if(expr == TOKEN_VOID){
        char msg[256];
        sprintf(msg, "Expressão sem valor (void) atribuída a '%s'", name);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // float aceita int, char e float
    if(dest == TOKEN_FLOAT && expr != TOKEN_FLOAT && expr != TOKEN_INT && expr != TOKEN_CHAR){
        char msg[256];
        const char *expr_str = (expr == TOKEN_INT)  ? "int"  :
                               (expr == TOKEN_CHAR) ? "char" :
                               (expr == TOKEN_STRING) ? "string" : "desconhecido";
        sprintf(msg,
                "Tipo incompatível: não é possível atribuir %s a '%s' (tipo float)",
                expr_str, name);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // Nenhum outro tipo pode receber float
    if(dest != TOKEN_FLOAT && expr == TOKEN_FLOAT){
        char msg[256];
        const char *dest_str = (dest == TOKEN_INT)  ? "int"  :
                               (dest == TOKEN_CHAR) ? "char" : "desconhecido";
        sprintf(msg,
                "Tipo incompatível: não é possível atribuir float a '%s' (tipo %s)",
                name, dest_str);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // string só é compatível com char
    if(expr == TOKEN_STRING && dest != TOKEN_CHAR){
        char msg[256];
        const char *dest_str = (dest == TOKEN_INT) ? "int" : "void";
        sprintf(msg,
                "Tipo incompatível: não é possível atribuir string a '%s' (tipo %s)",
                name, dest_str);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // int <> char: permitido de forma silenciosa
}

// =============================================================================
//  INFRAESTRUTURA DO PARSER
// =============================================================================

void inicializar_parser(Parser *parser, Lexer *lexer, CodeGen *cg){
    parser->lexer               = lexer;
    parser->current_token       = pegar_prox_token(lexer);
    parser->current_scope       = criar_escopo(NULL);
    parser->em_recuperacao      = 0;
    parser->quantidade_erros    = 0;
    parser->current_return_type = TOKEN_VOID;
    parser->last_reg            = -1;
    parser->cg                  = cg;
}

void avancar_token(Parser *parser){
    parser->current_token = pegar_prox_token(parser->lexer);
}

int erro_de_sintaxe(Parser *parser, const char *mensagem){
    if(parser->em_recuperacao) return 0;

    parser->em_recuperacao = 1;
    parser->quantidade_erros++;
    printf("\n");
    printf(RED "[ ERRO SINTÁTICO #%d ]\n" RESET, parser->quantidade_erros);
    printf(YELLOW "Mensagem:\n" RESET);
    printf("  %s\n\n", mensagem);
    printf(YELLOW "Localização:\n" RESET);
    printf("  Linha: %d\n", parser->current_token.line);
    printf("  Coluna: %d\n\n", parser->current_token.column);
    printf(YELLOW "Token encontrado:\n" RESET);
    printf("  Tipo: %s\n",   token_para_string(parser->current_token.type));
    printf("  Símbolo: %s\n", token_para_simbolo(parser->current_token.type));
    printf("  Lexema: \"%s\"\n\n", parser->current_token.lexema);
    printf(YELLOW "Trecho:\n" RESET);
    mostrar_linha_erro(parser);
    return 0;
}

int consumir_token(Parser *parser, TokenType tipo_esperado){
    if(parser->em_recuperacao) return 0;

    if(parser->current_token.type == tipo_esperado){
        avancar_token(parser);
        return 1;
    }

    char mensagem[200];
    sprintf(mensagem,
            "Esperado %s, mas encontrado %s",
            token_para_simbolo(tipo_esperado),
            token_para_simbolo(parser->current_token.type));
    return erro_de_sintaxe(parser, mensagem);
}

void sincronizar_parser(Parser *parser){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == TOKEN_SEMICOLON){
            avancar_token(parser);
            break;
        }
        if(parser->current_token.type == TOKEN_RBRACE) break;
        avancar_token(parser);
    }
    parser->em_recuperacao = 0;
}

void sincronizar_ate(Parser *parser, TokenType token){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == token){
            parser->em_recuperacao = 0;
            return;
        }
        avancar_token(parser);
    }
    parser->em_recuperacao = 0;
}

void mostrar_linha_erro(Parser *parser){
    const char *source = parser->lexer->src;
    int linha_atual = 1, i = 0;

    while(source[i] != '\0'){
        if(linha_atual == parser->current_token.line){
            printf(CYAN "%4d | " RESET, linha_atual);
            while(source[i] != '\n' && source[i] != '\0'){
                printf("%c", source[i++]);
            }
            printf("\n       ");
            for(int j = 1; j < parser->current_token.column; j++) printf(" ");
            printf(RED "^\n" RESET);
            return;
        }
        if(source[i] == '\n') linha_atual++;
        i++;
    }
}

const char *token_para_simbolo(TokenType type){
    switch(type){
        case TOKEN_INT:          return "int";
        case TOKEN_FLOAT:        return "float";    
        case TOKEN_CHAR:         return "char";
        case TOKEN_VOID:         return "void";
        case TOKEN_IF:           return "if";
        case TOKEN_ELSE:         return "else";
        case TOKEN_WHILE:        return "while";
        case TOKEN_FOR:          return "for";
        case TOKEN_RETURN:       return "return";
        case TOKEN_BREAK:        return "break";
        case TOKEN_CONTINUE:     return "continue";
        case TOKEN_PLUS:         return "+";
        case TOKEN_MINUS:        return "-";
        case TOKEN_MULT:         return "*";
        case TOKEN_DIV:          return "/";
        case TOKEN_ASSIGN:       return "=";
        case TOKEN_EQ:           return "==";
        case TOKEN_NEQ:          return "!=";
        case TOKEN_LT:           return "<";
        case TOKEN_GT:           return ">";
        case TOKEN_LTE:          return "<=";
        case TOKEN_GTE:          return ">=";
        case TOKEN_AND:          return "&&";
        case TOKEN_OR:           return "||";
        case TOKEN_NOT:          return "!";
        case TOKEN_INCREMENT:    return "++";
        case TOKEN_DECREMENT:    return "--";
        case TOKEN_LPAREN:       return "(";
        case TOKEN_RPAREN:       return ")";
        case TOKEN_LBRACE:       return "{";
        case TOKEN_RBRACE:       return "}";
        case TOKEN_COMMA:        return ",";
        case TOKEN_SEMICOLON:    return ";";
        case TOKEN_ID:           return "identificador";
        case TOKEN_NUM:          return "numero";
        case TOKEN_STRING:       return "string";
        case TOKEN_CHAR_LITERAL: return "char literal";
        case TOKEN_FLOAT_LITERAL: return "float literal";
        case TOKEN_EOF:          return "EOF";
        default:                 return "desconhecido";
    }
}

int token_eh_tipo(TokenType type){
    return type == TOKEN_INT || type == TOKEN_CHAR || type == TOKEN_VOID || type == TOKEN_FLOAT || type == TOKEN_STRING;
}

int token_eh_operador_relacional(TokenType type){
    return type == TOKEN_LT  || type == TOKEN_GT  ||
           type == TOKEN_LTE || type == TOKEN_GTE ||
           type == TOKEN_EQ  || type == TOKEN_NEQ;
}

int analisar_tipo(Parser *parser){
    if(parser->em_recuperacao) return 0;
    if(token_eh_tipo(parser->current_token.type)){
        avancar_token(parser);
        return 1;
    }
    return erro_de_sintaxe(parser, "Tipo esperado: int, char, float, string ou void");
}

// =============================================================================
//  EXPRESSÕES — Agora retornam TokenType ao invés de ASTNode*
//  analisar_fator:
//    - variável declarada?  (critério "declared")
//    - variável inicializada? (critério "initialized")
//    - marca como usada     (critério "used")
//    - retorna tipo da expressão (critério "types")
// =============================================================================

TokenType analisar_fator(Parser *parser) {
    if (parser->em_recuperacao) return TOKEN_ERROR;

    CodeGen *cg = parser->cg;

    // --- Operadores unários: - e ! ---
    if (parser->current_token.type == TOKEN_MINUS ||
        parser->current_token.type == TOKEN_NOT) {
        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        TokenType operand_type = analisar_fator(parser);
        if (operand_type == TOKEN_ERROR) return TOKEN_ERROR;

        if (cg) {
            int reg  = parser->last_reg;
            int novo = codegen_novo_reg(cg);
            if (op == TOKEN_MINUS)
                codegen_emitir(cg, "sub $t%d, $zero, $t%d", novo, reg);
            else
                codegen_emitir(cg, "seq $t%d, $t%d, $zero", novo, reg);
            parser->last_reg = novo;
        }

        return operador_unario_tipo(op, operand_type);
    }

    // --- Identificador ---
    if (parser->current_token.type == TOKEN_ID) {
        char name[100];
        strcpy(name, parser->current_token.lexema);
        int line   = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, TOKEN_ID);

        // Chamada de função
        if (parser->current_token.type == TOKEN_LPAREN) {
            analisar_chamada_funcao(parser, name, line, column);
            if (cg) {
                int reg = codegen_novo_reg(cg);
                codegen_emitir(cg, "move $t%d, $v0", reg);
                parser->last_reg = reg;
            }
            return TOKEN_INT;
        }

        // Uso de variável
        Symbol *sym = buscar_simbolo(parser, name);
        if (!sym) {
            char msg[256];
            sprintf(msg, "Variável '%s' não declarada", name);
            erro_semantico(parser, msg, line, column);
            return TOKEN_ERROR;
        }

        sym->used = 1;

        if (!sym->initialized) {
            char msg[256];
            sprintf(msg, "Variável '%s' usada sem ter sido inicializada", name);
            erro_semantico(parser, msg, line, column);
        }

        if (cg) {
            int reg = codegen_novo_reg(cg);
            codegen_emitir(cg, "lw $t%d, %s", reg, sym->label);
            parser->last_reg = reg;
        }

        return sym->type;
    }

    // --- Literal inteiro ---
    if (parser->current_token.type == TOKEN_NUM) {
        char valor[64];
        strcpy(valor, parser->current_token.lexema);
        consumir_token(parser, TOKEN_NUM);

        if (cg) {
            int reg = codegen_novo_reg(cg);
            codegen_emitir(cg, "li $t%d, %s", reg, valor);
            parser->last_reg = reg;
        }

        return TOKEN_INT;
    }

    // --- Literal char ---
    if (parser->current_token.type == TOKEN_CHAR_LITERAL) {
        char valor[64];
        strcpy(valor, parser->current_token.lexema);
        consumir_token(parser, TOKEN_CHAR_LITERAL);

        if (cg) {
            int reg = codegen_novo_reg(cg);
            codegen_emitir(cg, "li $t%d, %s", reg, valor);
            parser->last_reg = reg;
        }

        return TOKEN_CHAR;
    }

    // --- Literal float ---
    if (parser->current_token.type == TOKEN_FLOAT_LITERAL) {
        char valor[64];
        strcpy(valor, parser->current_token.lexema);
        consumir_token(parser, TOKEN_FLOAT_LITERAL);

        if (cg) {
            int reg = codegen_novo_reg(cg);
            codegen_emitir(cg, "# float %s (simplificado como int)", valor);
            codegen_emitir(cg, "li $t%d, 0", reg);
            parser->last_reg = reg;
        }

        return TOKEN_FLOAT;
    }

    // --- Literal string ---
    if (parser->current_token.type == TOKEN_STRING) {
        consumir_token(parser, TOKEN_STRING);
        parser->last_reg = -1;
        return TOKEN_STRING;
    }

    // --- Expressão entre parênteses ---
    if (parser->current_token.type == TOKEN_LPAREN) {
        consumir_token(parser, TOKEN_LPAREN);
        if (parser->current_token.type == TOKEN_RPAREN) {
            erro_de_sintaxe(parser, "Expressão vazia entre parênteses");
            return TOKEN_ERROR;
        }
        TokenType tipo = analisar_expressao(parser);
        if (parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
        consumir_token(parser, TOKEN_RPAREN);
        // last_reg já foi setado por analisar_expressao, não precisa mexer
        return tipo;
    }

    erro_de_sintaxe(parser, "Expressão inválida: fator ausente ou operador isolado");
    return TOKEN_ERROR;
}

TokenType analisar_termo(Parser *parser) {
    if (parser->em_recuperacao) return TOKEN_ERROR;

    TokenType left_type = analisar_fator(parser);
    if (left_type == TOKEN_ERROR) return TOKEN_ERROR;

    CodeGen *cg = parser->cg;

    while (parser->current_token.type == TOKEN_MULT ||
           parser->current_token.type == TOKEN_DIV) {
        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        int left_reg = parser->last_reg;

        TokenType right_type = analisar_fator(parser);
        if (right_type == TOKEN_ERROR) return TOKEN_ERROR;

        if (cg) {
            int right_reg = parser->last_reg;
            int resultado = codegen_novo_reg(cg);
            if (op == TOKEN_MULT)
                codegen_emitir(cg, "mul $t%d, $t%d, $t%d", resultado, left_reg, right_reg);
            else
                codegen_emitir(cg, "div $t%d, $t%d, $t%d", resultado, left_reg, right_reg);
            parser->last_reg = resultado;
        }

        left_type = operador_binario_tipo(op, left_type, right_type);
    }

    return left_type;
}

TokenType analisar_expressao(Parser *parser) {
    if (parser->em_recuperacao) return TOKEN_ERROR;

    TokenType left_type = analisar_termo(parser);
    if (left_type == TOKEN_ERROR) return TOKEN_ERROR;

    CodeGen *cg = parser->cg;

    while (parser->current_token.type == TOKEN_PLUS ||
           parser->current_token.type == TOKEN_MINUS) {
        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        int left_reg = parser->last_reg;

        TokenType right_type = analisar_termo(parser);
        if (right_type == TOKEN_ERROR) return TOKEN_ERROR;

        if (cg) {
            int right_reg = parser->last_reg;
            int resultado = codegen_novo_reg(cg);
            if (op == TOKEN_PLUS)
                codegen_emitir(cg, "add $t%d, $t%d, $t%d", resultado, left_reg, right_reg);
            else
                codegen_emitir(cg, "sub $t%d, $t%d, $t%d", resultado, left_reg, right_reg);
            parser->last_reg = resultado;
        }

        left_type = operador_binario_tipo(op, left_type, right_type);
    }

    return left_type;
}


TokenType analisar_condicao_relacional(Parser *parser){
    if(parser->em_recuperacao) return TOKEN_ERROR;

    TokenType left_type = analisar_expressao(parser);
    if(left_type == TOKEN_ERROR) return TOKEN_ERROR;

    if(token_eh_operador_relacional(parser->current_token.type)){
        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        int left_reg = parser->last_reg;

        TokenType right_type = analisar_expressao(parser);
        if(right_type == TOKEN_ERROR) return TOKEN_ERROR;

        if(parser->cg){
            int right_reg = parser->last_reg;
            int resultado = codegen_novo_reg(parser->cg);

            const char *instrucao;
            switch(op){
                case TOKEN_LT:  instrucao = "slt"; break;
                case TOKEN_GT:  instrucao = "sgt"; break;
                case TOKEN_LTE: instrucao = "sle"; break;
                case TOKEN_GTE: instrucao = "sge"; break;
                case TOKEN_EQ:  instrucao = "seq"; break;
                case TOKEN_NEQ: instrucao = "sne"; break;
                default:        instrucao = "seq"; break;
            }

            codegen_emitir(parser->cg, "%s $t%d, $t%d, $t%d",
                           instrucao, resultado, left_reg, right_reg);
            parser->last_reg = resultado;
        }

        return operador_binario_tipo(op, left_type, right_type);
    }

    return left_type;
}

TokenType analisar_condicao(Parser *parser){
    if(parser->em_recuperacao) return TOKEN_ERROR;

    TokenType left_type = analisar_condicao_relacional(parser);
    if(left_type == TOKEN_ERROR) return TOKEN_ERROR;

    while(parser->current_token.type == TOKEN_AND ||
          parser->current_token.type == TOKEN_OR){
        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        int left_reg = parser->last_reg;

        TokenType right_type = analisar_condicao_relacional(parser);
        if(right_type == TOKEN_ERROR) return TOKEN_ERROR;

        if(parser->cg){
            int right_reg = parser->last_reg;
            int resultado = codegen_novo_reg(parser->cg);

            if(op == TOKEN_AND)
                codegen_emitir(parser->cg, "and $t%d, $t%d, $t%d",
                               resultado, left_reg, right_reg);
            else
                codegen_emitir(parser->cg, "or $t%d, $t%d, $t%d",
                               resultado, left_reg, right_reg);

            parser->last_reg = resultado;
        }

        left_type = operador_binario_tipo(op, left_type, right_type);
    }

    return left_type;
}

void analisar_operador_relacional(Parser *parser){
    if(token_eh_operador_relacional(parser->current_token.type))
        avancar_token(parser);
    else
        erro_de_sintaxe(parser, "Operador relacional ausente na condição");
}

void analisar_lista_de_argumentos(Parser *parser){
    if(parser->em_recuperacao) return;

    TokenType expr_type = analisar_expressao(parser);
    if(expr_type == TOKEN_ERROR) return;

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);
        if(parser->current_token.type == TOKEN_RPAREN){
            erro_de_sintaxe(parser, "Falta uma expressão após a vírgula");
            return;
        }
        expr_type = analisar_expressao(parser);
        if(expr_type == TOKEN_ERROR) return;
    }
}

void analisar_chamada_funcao(Parser *parser, const char *name __attribute__((unused)), 
                               int line __attribute__((unused)), int column __attribute__((unused))){
    if(parser->em_recuperacao) return;

    if(!consumir_token(parser, TOKEN_LPAREN)){
        return;
    }
    if(parser->current_token.type != TOKEN_RPAREN){
        analisar_lista_de_argumentos(parser);
        if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    }
    consumir_token(parser, TOKEN_RPAREN);
}

// =============================================================================
//  DECLARAÇÕES
//  analisar_declaracao agora:
//    1. Checa se o identificador já existe no escopo (redeclaração).
//    2. Valida o tipo da expressão de inicialização (critério "types").
//    3. Registra a variável com o flag `initialized` correto.
// =============================================================================

void analisar_declaracao(Parser *parser){
    if(parser->em_recuperacao) return;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return;
    }
    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na declaração");
        sincronizar_parser(parser);
        return;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    int initialized = 0;

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        TokenType expr_type = analisar_expressao(parser);

        if(expr_type != TOKEN_ERROR){
            initialized = 1;
            validar_tipos(parser, name, declared_type, expr_type, line, column);
        } else {
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            declarar_variavel(parser, name, declared_type, 0, line, column);
            free(name);

            if (parser->cg) codegen_resetar_regs(parser->cg);
            return;
        }
    }

    Symbol *sym_decl = declarar_variavel(parser, name, declared_type, initialized, line, column);
    free(name);

    // sw só se houve inicializador, símbolo criado com sucesso e registrador válido
    if (parser->cg && parser->last_reg >= 0 && sym_decl && initialized) {
        codegen_emitir(parser->cg, "sw $t%d, %s", parser->last_reg, sym_decl->label);
    }

    if(!consumir_token(parser, TOKEN_SEMICOLON)){
        parser->em_recuperacao = 0;

        if (parser->cg) codegen_resetar_regs(parser->cg);
        return;
    }

    if (parser->cg) codegen_resetar_regs(parser->cg);
}

// Versão sem ponto-e-vírgula usada na inicialização do for
void analisar_declaracao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)) return;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na declaração");
        return;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    int initialized = 0;

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        TokenType expr_type = analisar_expressao(parser);
        if(expr_type != TOKEN_ERROR){
            initialized = 1;
            validar_tipos(parser, name, declared_type, expr_type, line, column);
        }
    }

    declarar_variavel(parser, name, declared_type, initialized, line, column);
    free(name);
}

// =============================================================================
//  ATRIBUIÇÕES
//  analisar_atribuicao agora:
//    1. Verifica se a variável foi declarada (critério "declared").
//    2. Marca como inicializada (critério "initialized").
//    3. Valida compatibilidade de tipos (critério "types").
// =============================================================================

void analisar_atribuicao(Parser *parser){
    if(parser->em_recuperacao) return;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na atribuição");
        sincronizar_parser(parser);
        return;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);

    TokenType expr_type = analisar_expressao(parser);

    Symbol *sym = buscar_simbolo(parser, name);
    if(!sym){
        char msg[256];
        sprintf(msg, "Atribuição a variável '%s' não declarada", name);
        erro_semantico(parser, msg, line, column);
    } else {
        sym->initialized = 1;
        if(expr_type != TOKEN_ERROR){
            validar_tipos(parser, name, sym->type, expr_type, line, column);

            // Só emite sw se a variável existe (senão não há endereço de memória válido)
            if (parser->cg && parser->last_reg >= 0) {
                codegen_emitir(parser->cg, "sw $t%d, %s", parser->last_reg, sym->label);
            }
        }
    }

    free(name);

    if(expr_type == TOKEN_ERROR){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
        consumir_token(parser, TOKEN_SEMICOLON);

        if (parser->cg) codegen_resetar_regs(parser->cg);
        return;
    }

    consumir_token(parser, TOKEN_SEMICOLON);

    if (parser->cg) codegen_resetar_regs(parser->cg);
}

// Versão sem ponto-e-vírgula usada na atribuição do for

void analisar_atribuicao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na atribuição");
        return;   // nada emitido ainda -> sem reset
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);

    TokenType expr_type = analisar_expressao(parser);   // <- pode emitir regs a partir daqui

    Symbol *sym = buscar_simbolo(parser, name);
    if(!sym){
        char msg[256];
        sprintf(msg, "Atribuição a variável '%s' não declarada", name);
        erro_semantico(parser, msg, line, column);
    } else {
        sym->initialized = 1;
        if(expr_type != TOKEN_ERROR){
            validar_tipos(parser, name, sym->type, expr_type, line, column);

            if (parser->cg && parser->last_reg >= 0) {
                codegen_emitir(parser->cg, "sw $t%d, %s", parser->last_reg, sym->label);
            }
        }
    }
    free(name);

    if (parser->cg) codegen_resetar_regs(parser->cg);   // único caminho de saída após a expressão
}

// =============================================================================
//  COMANDO INICIADO POR IDENTIFICADOR
//  Distingue atribuição, chamada de função e pós-incremento/decremento.
// =============================================================================

void analisar_comando_iniciado_por_id(Parser *parser){
    if(parser->em_recuperacao) return;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no comando");
        sincronizar_parser(parser);
        return;   // nada emitido -> sem reset
    }

    char *name = strdup(parser->current_token.lexema);
    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    consumir_token(parser, TOKEN_ID);

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        TokenType expr_type = analisar_expressao(parser);   // <- pode emitir regs

        Symbol *sym = buscar_simbolo(parser, name);
        if(!sym){
            char msg[256];
            sprintf(msg, "Atribuição a variável '%s' não declarada", name);
            erro_semantico(parser, msg, line, column);
        } else {
            sym->initialized = 1;
            if(expr_type != TOKEN_ERROR){
                validar_tipos(parser, name, sym->type, expr_type, line, column);

                if (parser->cg && parser->last_reg >= 0) {
                    codegen_emitir(parser->cg, "sw $t%d, %s", parser->last_reg, sym->label);
                }
            }
        }

        if(expr_type == TOKEN_ERROR){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            free(name);

            if (parser->cg) codegen_resetar_regs(parser->cg);   // expressão falhou, mas pode ter gasto regs
            return;
        }
        consumir_token(parser, TOKEN_SEMICOLON);

        if (parser->cg) codegen_resetar_regs(parser->cg);   // caminho de sucesso da atribuição

    } else if(parser->current_token.type == TOKEN_LPAREN){
        analisar_chamada_funcao(parser, name, line, column);
        consumir_token(parser, TOKEN_SEMICOLON);

        // chamada de função pode emitir código (a versão atual de analisar_fator
        // já gera "move $tN, $v0" para chamadas usadas em expressão, mas aqui dentro
        // de analisar_lista_de_argumentos cada argumento já é uma analisar_expressao)
        if (parser->cg) codegen_resetar_regs(parser->cg);

    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        consumir_token(parser, parser->current_token.type);

        Symbol *sym = buscar_simbolo(parser, name);
        if(!sym){
            char msg[256];
            sprintf(msg, "Variável '%s' não declarada", name);
            erro_semantico(parser, msg, line, column);
        } else {
            sym->initialized = 1;
            sym->used        = 1;
        }

        consumir_token(parser, TOKEN_SEMICOLON);
        // nada emitido neste ramo ainda (++/-- não gera código por enquanto) -> sem reset necessário,
        // mas não faz mal deixar por segurança/consistência:
        if (parser->cg) codegen_resetar_regs(parser->cg);

    } else {
        erro_de_sintaxe(parser,
            "Esperado atribuição, chamada de função, ++ ou -- após o identificador");
        sincronizar_parser(parser);
        free(name);
        return;   // erro de sintaxe, nada emitido -> sem reset
    }

    free(name);
}

// =============================================================================
//  COMANDOS COMPOSTOS
// =============================================================================

void analisar_comando(Parser *parser){
    if(parser->em_recuperacao) return;

    if(token_eh_tipo(parser->current_token.type)){
        analisar_declaracao(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_ID){
        analisar_comando_iniciado_por_id(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_LBRACE){
        analisar_bloco(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_IF){
        analisar_if(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_WHILE){
        analisar_while(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_FOR){
        analisar_for(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_RETURN){
        analisar_return(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_BREAK){
        analisar_break(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_CONTINUE){
        analisar_continue(parser);
        return;
    }

    erro_de_sintaxe(parser, "Comando inválido ou não suportado");
    sincronizar_parser(parser);
}

void analisar_lista_de_comandos(Parser *parser){
    while(parser->current_token.type != TOKEN_RBRACE &&
          parser->current_token.type != TOKEN_EOF){
        analisar_comando(parser);
        if(parser->em_recuperacao) sincronizar_parser(parser);
    }
}

// =============================================================================
//  BLOCO
//  Abre e fecha um novo escopo
// =============================================================================

void analisar_bloco(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!consumir_token(parser, TOKEN_LBRACE)){
        sincronizar_parser(parser);
        return;
    }

    entrar_escopo(parser);
    analisar_lista_de_comandos(parser);
    consumir_token(parser, TOKEN_RBRACE);
    sair_escopo(parser);
}

// =============================================================================
//  PARÂMETROS
// =============================================================================

void analisar_parametro(Parser *parser){
    if(parser->em_recuperacao) return;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_ate(parser, TOKEN_RPAREN);
        return;
    }
    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no parâmetro");
        sincronizar_ate(parser, TOKEN_RPAREN);
        return;
    }

    char *name = strdup(parser->current_token.lexema);
    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_ID);

    // Parâmetros são inicializados por definição
    declarar_variavel(parser, name, declared_type, 1, line, column);
    free(name);
}

void analisar_lista_de_parametros(Parser *parser){
    if(parser->em_recuperacao) return;

    analisar_parametro(parser);

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);
        if(parser->current_token.type != TOKEN_RPAREN){
            analisar_parametro(parser);
        }
    }
}

// =============================================================================
//  ESTRUTURAS DE CONTROLE
// =============================================================================

void analisar_if(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_IF);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return;
    }

    analisar_condicao(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    consumir_token(parser, TOKEN_RPAREN);

    if(parser->cg){
        int cond_reg = parser->last_reg;
        int label_id = codegen_novo_label(parser->cg);

        // reserva os nomes dos labels antes de entrar no corpo
        char l_else[32], l_end[32];
        snprintf(l_else, sizeof(l_else), "L_else_%d", label_id);
        snprintf(l_end,  sizeof(l_end),  "L_end_%d",  label_id);

        int tem_else = 0;

        // salta para else (ou end) se condição for falsa
        codegen_emitir(parser->cg, "beq $t%d, $zero, %s",
                       cond_reg, l_else);
        codegen_resetar_regs(parser->cg);

        analisar_comando(parser);  // corpo do if

        // verifica se tem else antes de emitir o j
        if(parser->current_token.type == TOKEN_ELSE){
            tem_else = 1;
            codegen_emitir(parser->cg, "j %s", l_end);
        }

        codegen_emitir_label(parser->cg, l_else);

        if(tem_else){
            consumir_token(parser, TOKEN_ELSE);
            codegen_resetar_regs(parser->cg);
            analisar_comando(parser);  // corpo do else
            codegen_emitir_label(parser->cg, l_end);
        }

        codegen_resetar_regs(parser->cg);

    } else {
        // sem codegen, comportamento original
        analisar_comando(parser);
        if(parser->current_token.type == TOKEN_ELSE){
            consumir_token(parser, TOKEN_ELSE);
            analisar_comando(parser);
        }
    }
}

void analisar_while(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_WHILE);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return;
    }

    analisar_condicao(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    consumir_token(parser, TOKEN_RPAREN);

    analisar_comando(parser);
}

// =============================================================================
//  FOR
// =============================================================================

void analisar_inicializacao_for(Parser *parser){
    if(parser->em_recuperacao) return;

    if(token_eh_tipo(parser->current_token.type)){
        analisar_declaracao_sem_ponto_virgula(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_ID){
        analisar_atribuicao_sem_ponto_virgula(parser);
        return;
    }

    erro_de_sintaxe(parser, "Inicialização inválida no for");
}

void analisar_expressao_de_incremento(Parser *parser){
    if(parser->em_recuperacao) return;

    if(parser->current_token.type == TOKEN_ID){
        int   line   = parser->current_token.line;
        int   column = parser->current_token.column;
        char *name   = strdup(parser->current_token.lexema);
        consumir_token(parser, TOKEN_ID);

        if(parser->current_token.type == TOKEN_INCREMENT ||
           parser->current_token.type == TOKEN_DECREMENT){
            consumir_token(parser, parser->current_token.type);

            Symbol *sym = buscar_simbolo(parser, name);
            if(!sym){
                char msg[256];
                sprintf(msg, "Variável '%s' não declarada", name);
                erro_semantico(parser, msg, line, column);
            } else {
                sym->initialized = 1;
                sym->used        = 1;
            }
            // nada emitido (++ ainda não gera código) -> sem reset necessário

        } else if(parser->current_token.type == TOKEN_ASSIGN){
            consumir_token(parser, TOKEN_ASSIGN);
            TokenType expr_type = analisar_expressao(parser);   // <- pode emitir regs

            Symbol *sym = buscar_simbolo(parser, name);
            if(!sym){
                char msg[256];
                sprintf(msg, "Variável '%s' não declarada", name);
                erro_semantico(parser, msg, line, column);
            } else {
                sym->initialized = 1;
                if(expr_type != TOKEN_ERROR){
                    validar_tipos(parser, name, sym->type, expr_type, line, column);

                    if (parser->cg && parser->last_reg >= 0) {
                        codegen_emitir(parser->cg, "sw $t%d, %s", parser->last_reg, sym->label);
                    }
                }
            }

            if (parser->cg) codegen_resetar_regs(parser->cg);   // único caminho após a expressão

        } else {
            erro_de_sintaxe(parser, "Esperado ++, -- ou = na parte de incremento do for");
        }

        free(name);
        return;

    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        // pré-incremento/decremento — não emite código ainda, sem reset necessário
        consumir_token(parser, parser->current_token.type);

        if(parser->current_token.type != TOKEN_ID){
            erro_de_sintaxe(parser, "Esperado identificador após ++/--");
            return;
        }

        const char *name  = parser->current_token.lexema;
        int id_line       = parser->current_token.line;
        int id_column     = parser->current_token.column;

        Symbol *sym = buscar_simbolo(parser, name);
        if(!sym){
            char msg[256];
            sprintf(msg, "Variável '%s' não declarada", name);
            erro_semantico(parser, msg, id_line, id_column);
        } else {
            sym->initialized = 1;
            sym->used        = 1;
        }

        consumir_token(parser, TOKEN_ID);
    }
}

void analisar_for(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_FOR);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return;
    }

    // Escopo do for
    entrar_escopo(parser);

    analisar_inicializacao_for(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_SEMICOLON);
    consumir_token(parser, TOKEN_SEMICOLON);

    analisar_condicao(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_SEMICOLON);
    consumir_token(parser, TOKEN_SEMICOLON);

    analisar_expressao_de_incremento(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);

    if(!consumir_token(parser, TOKEN_RPAREN)){
        if(parser->current_token.type == TOKEN_LBRACE){
            parser->em_recuperacao = 0;
        } else {
            sincronizar_ate(parser, TOKEN_RPAREN);
            if(parser->current_token.type == TOKEN_RPAREN)
                consumir_token(parser, TOKEN_RPAREN);
            else {
                sair_escopo(parser);
                sincronizar_parser(parser);
                return;
            }
        }
    }

    analisar_comando(parser);
    sair_escopo(parser);
}

void analisar_return(Parser *parser){
    if(parser->em_recuperacao) return;

    int line   = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_RETURN);

    if(parser->current_token.type != TOKEN_SEMICOLON){
        TokenType expr_type = analisar_expressao(parser);
        if(expr_type == TOKEN_ERROR){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
        } else {
            TokenType ret_type = parser->current_return_type;

            if(ret_type == TOKEN_VOID){
                char msg[256];
                sprintf(msg,
                    "Função void não pode retornar um valor (linha %d)", line);
                erro_semantico(parser, msg, line, column);
            } else if(expr_type != TOKEN_ERROR){
                validar_tipos(parser, "<retorno>", ret_type, expr_type, line, column);
            }
        }
    } else {
        // return; sem valor
        if(parser->current_return_type != TOKEN_VOID &&
           parser->current_return_type != TOKEN_ERROR){
            char msg[256];
            sprintf(msg, "Função não-void deve retornar um valor");
            erro_semantico(parser, msg, line, column);
        }
    }

    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_break(Parser *parser){
    if(parser->em_recuperacao) return;
    consumir_token(parser, TOKEN_BREAK);
    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_continue(Parser *parser){
    if(parser->em_recuperacao) return;
    consumir_token(parser, TOKEN_CONTINUE);
    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_incremento_decremento(Parser *parser){
    if(parser->em_recuperacao) return;

    if(parser->current_token.type == TOKEN_INCREMENT ||
       parser->current_token.type == TOKEN_DECREMENT){
        consumir_token(parser, parser->current_token.type);
    } else {
        erro_de_sintaxe(parser, "Esperado ++ ou --");
    }
}

// =============================================================================
//  FUNÇÕES
// =============================================================================

void analisar_funcao(Parser *parser){
    if(parser->em_recuperacao) return;

    TokenType return_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return;
    }
    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no nome da função");
        sincronizar_parser(parser);
        return;
    }

    char *name = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        free(name);
        return;
    }

    // Escopo da função
    entrar_escopo(parser);

    if(parser->current_token.type != TOKEN_RPAREN){
        analisar_lista_de_parametros(parser);
        if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    }

    // Salva tipo de retorno
    TokenType tipo_retorno_anterior = parser->current_return_type;
    parser->current_return_type = return_type;

    consumir_token(parser, TOKEN_RPAREN);
    analisar_bloco(parser);
    sair_escopo(parser);

    // Restaura tipo de retorno
    parser->current_return_type = tipo_retorno_anterior;
    free(name);
}

void analisar_lista_de_funcoes(Parser *parser){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == TOKEN_RBRACE){
            erro_de_sintaxe(parser,
                "Chave de fechamento inesperada no escopo global");
            avancar_token(parser);
            parser->em_recuperacao = 0;
            continue;
        }

        analisar_funcao(parser);

        if(parser->em_recuperacao){
            sincronizar_parser(parser);
            if(parser->current_token.type == TOKEN_RBRACE){
                avancar_token(parser);
                parser->em_recuperacao = 0;
            }
        }
    }
}

int analisar_programa(Parser *parser){
    analisar_lista_de_funcoes(parser);
    consumir_token(parser, TOKEN_EOF);

    if (parser->cg && parser->quantidade_erros == 0) {
        codegen_finalizar(parser->cg);
    }

    return parser->quantidade_erros == 0 ? 1 : 0;
}