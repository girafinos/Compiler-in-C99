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
    return s;
}

// -----------------------------------------------------------------------------
//  Auxiliar semântico: infere o tipo resultante de um nó de expressão.
//  Usado para validar atribuições (critério "types").
// -----------------------------------------------------------------------------

static TokenType inferir_tipo(Parser *parser, ASTNode *node){
    if(!node) return TOKEN_ERROR;
    switch(node->type){
        case AST_LITERAL:
            if(node->value_type == TOKEN_FLOAT_LITERAL) return TOKEN_FLOAT;
            return node->value_type;

        case AST_IDENTIFIER: {
            // Prefere o tipo resolvido na tabela de símbolos ao value_type do nó.
            Symbol *sym = buscar_simbolo(parser, node->name);
            if(sym) return sym->type;
            return node->value_type;
        }

        case AST_UNARY_EXPR:
            if(node->value_type == TOKEN_NOT) return TOKEN_INT;
            return inferir_tipo(parser, node->left);

        case AST_BINARY_EXPR: {
            TokenType op = node->value_type;
            // Relacionais e lógicos sempre produzem int.
            if(op == TOKEN_AND || op == TOKEN_OR  ||
               op == TOKEN_EQ  || op == TOKEN_NEQ ||
               op == TOKEN_LT  || op == TOKEN_GT  ||
               op == TOKEN_LTE || op == TOKEN_GTE)
                return TOKEN_INT;

            TokenType l = inferir_tipo(parser, node->left);
            TokenType r = inferir_tipo(parser, node->right);
            if(l == TOKEN_ERROR || r == TOKEN_ERROR) return TOKEN_ERROR;

            // Qualquer operação com float produz float.
            if(l == TOKEN_FLOAT || r == TOKEN_FLOAT) return TOKEN_FLOAT;
            if(l == TOKEN_INT || r == TOKEN_FLOAT) return TOKEN_INT;
            // int prevalece sobre char.
            if(l == TOKEN_INT || r == TOKEN_INT) return TOKEN_INT;
            return TOKEN_CHAR;
        }

        case AST_FUNCTION_CALL:
            // Sem tabela de funções, assume int como retorno padrão.
            return TOKEN_INT;

        default:
            return TOKEN_ERROR;
    }
}

// -----------------------------------------------------------------------------
//  Auxiliar semântico: valida compatibilidade de tipos numa atribuição.
//  Critério "types" do professor.
// -----------------------------------------------------------------------------

static void validar_tipos(Parser *parser,
                           const char *name,
                           TokenType dest, TokenType expr,
                           int line, int column){
    if(expr == TOKEN_ERROR) return;

    // void nunca recebe valor.
    if(dest == TOKEN_VOID){
        char msg[256];
        sprintf(msg, "Atribuição inválida: variável '%s' é do tipo void", name);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // Expressão void não pode ser atribuída.
    if(expr == TOKEN_VOID){
        char msg[256];
        sprintf(msg, "Expressão sem valor (void) atribuída a '%s'", name);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // float é isolado: só aceita float.
    if(dest == TOKEN_FLOAT && expr != TOKEN_FLOAT){
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

    // Nenhum outro tipo pode receber float.
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

    // string só é compatível com char (literais de inicialização simples).
    if(expr == TOKEN_STRING && dest != TOKEN_CHAR){
        char msg[256];
        const char *dest_str = (dest == TOKEN_INT) ? "int" : "void";
        sprintf(msg,
                "Tipo incompatível: não é possível atribuir string a '%s' (tipo %s)",
                name, dest_str);
        erro_semantico(parser, msg, line, column);
        return;
    }

    // int ↔ char: permitido silenciosamente 
}

// =============================================================================
//  INFRAESTRUTURA DO PARSER
// =============================================================================

void inicializar_parser(Parser *parser, Lexer *lexer){
    parser->lexer            = lexer;
    parser->current_token    = pegar_prox_token(lexer);
    parser->current_scope    = criar_escopo(NULL); 
    parser->em_recuperacao   = 0;
    parser->quantidade_erros = 0;
    parser->current_return_type = TOKEN_VOID; 
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
//  EXPRESSÕES
//  analisar_fator:
//    - variável declarada?  (critério "declared")
//    - variável inicializada? (critério "initialized")
//    - marca como usada     (critério "used")
//    - propaga tipo para validação de atribuição (critério "types")
// =============================================================================

ASTNode *analisar_fator(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    // --- Operadores unários: - e ! ---
    if(parser->current_token.type == TOKEN_MINUS ||
       parser->current_token.type == TOKEN_NOT){
        TokenType op  = parser->current_token.type;
        int line      = parser->current_token.line;
        int column    = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *operand = analisar_fator(parser);
        if(!operand) return NULL;

        ASTNode *node    = ast_novo_no(AST_UNARY_EXPR, line, column);
        node->value_type = op;
        node->left       = operand;
        return node;
    }

    // --- Identificador: variável ou chamada de função ---
    if(parser->current_token.type == TOKEN_ID){
        char name[100];
        strcpy(name, parser->current_token.lexema);
        int line   = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, TOKEN_ID);

        // Chamada de função — análise semântica de uso não se aplica ao nome.
        if(parser->current_token.type == TOKEN_LPAREN)
            return analisar_chamada_funcao(parser, name, line, column);

        // Uso de variável
        ASTNode *identifier = ast_novo_identificador(name, line, column);
        Symbol  *sym        = buscar_simbolo(parser, name);

        if(!sym){
            // Critério "declared": variável usada sem ter sido declarada.
            char msg[256];
            sprintf(msg, "Variável '%s' não declarada", name);
            erro_semantico(parser, msg, line, column);
            return identifier;
        }

        // Critério "used": marca que a variável foi lida.
        sym->used = 1;

        // Critério "initialized": avisa se está sendo lida sem valor.
        if(!sym->initialized){
            char msg[256];
            sprintf(msg, "Variável '%s' usada sem ter sido inicializada", name);
            erro_semantico(parser, msg, line, column);
        }

        // Propaga o tipo para que expressões pai possam validar atribuições.
        identifier->value_type = sym->type;
        return identifier;
    }

    // --- Literais numéricos, char e string ---
    if(parser->current_token.type == TOKEN_NUM       ||
       parser->current_token.type == TOKEN_CHAR_LITERAL ||
       parser->current_token.type == TOKEN_STRING ||
       parser->current_token.type == TOKEN_FLOAT_LITERAL ){
        ASTNode *literal = ast_novo_literal(parser->current_token.lexema,
                           parser->current_token.type,
                           parser->current_token.line,
                           parser->current_token.column);
        consumir_token(parser, parser->current_token.type);
        return literal;
    }

    // --- Expressão entre parênteses ---
    if(parser->current_token.type == TOKEN_LPAREN){
        consumir_token(parser, TOKEN_LPAREN);
        if(parser->current_token.type == TOKEN_RPAREN){
            erro_de_sintaxe(parser, "Expressão vazia entre parênteses");
            return NULL;
        }
        ASTNode *expression = analisar_expressao(parser);
        if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
        consumir_token(parser, TOKEN_RPAREN);
        return expression;
    }

    erro_de_sintaxe(parser, "Expressão inválida: fator ausente ou operador isolado");
    return NULL;
}

ASTNode *analisar_termo(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_fator(parser);
    if(!left) return NULL;

    while(parser->current_token.type == TOKEN_MULT ||
          parser->current_token.type == TOKEN_DIV){
        TokenType op  = parser->current_token.type;
        int line      = parser->current_token.line;
        int column    = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_fator(parser);
        if(!right) return NULL;

        ASTNode *binary    = ast_novo_no(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left       = left;
        binary->right      = right;
        left = binary;
    }
    return left;
}

ASTNode *analisar_expressao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_termo(parser);
    if(!left) return NULL;

    while(parser->current_token.type == TOKEN_PLUS ||
          parser->current_token.type == TOKEN_MINUS){
        TokenType op  = parser->current_token.type;
        int line      = parser->current_token.line;
        int column    = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_termo(parser);
        if(!right) return NULL;

        ASTNode *binary    = ast_novo_no(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left       = left;
        binary->right      = right;
        left = binary;
    }
    return left;
}

ASTNode *analisar_condicao_relacional(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_expressao(parser);
    if(!left) return NULL;

    if(token_eh_operador_relacional(parser->current_token.type)){
        TokenType op  = parser->current_token.type;
        int line      = parser->current_token.line;
        int column    = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_expressao(parser);
        if(!right) return NULL;

        ASTNode *binary    = ast_novo_no(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left       = left;
        binary->right      = right;
        return binary;
    }
    return left;
}

ASTNode *analisar_condicao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_condicao_relacional(parser);
    if(!left) return NULL;

    while(parser->current_token.type == TOKEN_AND ||
          parser->current_token.type == TOKEN_OR){
        TokenType op  = parser->current_token.type;
        int line      = parser->current_token.line;
        int column    = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_condicao_relacional(parser);
        if(!right) return NULL;

        ASTNode *binary    = ast_novo_no(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left       = left;
        binary->right      = right;
        left = binary;
    }
    return left;
}

void analisar_operador_relacional(Parser *parser){
    if(token_eh_operador_relacional(parser->current_token.type))
        avancar_token(parser);
    else
        erro_de_sintaxe(parser, "Operador relacional ausente na condição");
}

ASTNode *analisar_lista_de_argumentos(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *arguments = NULL;
    ASTNode *expr      = analisar_expressao(parser);
    if(!expr) return NULL;
    arguments = ast_anexar(arguments, expr);

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);
        if(parser->current_token.type == TOKEN_RPAREN){
            erro_de_sintaxe(parser, "Falta uma expressão após a vírgula");
            return NULL;
        }
        expr = analisar_expressao(parser);
        if(!expr) return NULL;
        arguments = ast_anexar(arguments, expr);
    }
    return arguments;
}

ASTNode *analisar_chamada_funcao(Parser *parser, const char *name, int line, int column){
    if(parser->em_recuperacao) return NULL;

    ASTNode *call = ast_novo_no(AST_FUNCTION_CALL, line, column);
    if(!call) return NULL;
    call->name = strdup(name);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        ast_liberar(call);
        return NULL;
    }
    if(parser->current_token.type != TOKEN_RPAREN){
        call->left = analisar_lista_de_argumentos(parser);
        if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    }
    consumir_token(parser, TOKEN_RPAREN);
    return call;
}

// =============================================================================
//  DECLARAÇÕES
//  analisar_declaracao agora:
//    1. Checa se o identificador já existe no escopo (redeclaração).
//    2. Valida o tipo da expressão de inicialização (critério "types").
//    3. Registra a variável com o flag `initialized` correto.
// =============================================================================

ASTNode *analisar_declaracao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return NULL;
    }
    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na declaração");
        sincronizar_parser(parser);
        return NULL;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    ASTNode *decl    = ast_novo_no(AST_DECLARATION, line, column);
    decl->name       = name;
    decl->value_type = declared_type;

    int initialized = 0;

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        decl->left = analisar_expressao(parser);

        if(decl->left){
            initialized = 1;
            // Critério "types": valida compatibilidade entre declaração e valor.
            TokenType expr_type = inferir_tipo(parser, decl->left);
            validar_tipos(parser, name, declared_type, expr_type, line, column);
        } else {
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            // Registra mesmo sem valor para não causar falso "não declarada".
            declarar_variavel(parser, name, declared_type, 0, line, column);
            return decl;
        }
    }

    // Critério "declared": registra no escopo corrente.
    declarar_variavel(parser, name, declared_type, initialized, line, column);

    if(!consumir_token(parser, TOKEN_SEMICOLON)){
        parser->em_recuperacao = 0;
        return decl;
    }
    return decl;
}

// Versão sem ponto-e-vírgula usada na inicialização do for.
ASTNode *analisar_declaracao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na declaração");
        return NULL;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    ASTNode *decl    = ast_novo_no(AST_DECLARATION, line, column);
    decl->name       = name;
    decl->value_type = declared_type;

    int initialized = 0;

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        decl->left = analisar_expressao(parser);
        if(decl->left){
            initialized = 1;
            TokenType expr_type = inferir_tipo(parser, decl->left);
            validar_tipos(parser, name, declared_type, expr_type, line, column);
        }
    }

    declarar_variavel(parser, name, declared_type, initialized, line, column);
    return decl;
}

// =============================================================================
//  ATRIBUIÇÕES
//  analisar_atribuicao agora:
//    1. Verifica se a variável foi declarada (critério "declared").
//    2. Marca como inicializada (critério "initialized").
//    3. Valida compatibilidade de tipos (critério "types").
// =============================================================================

ASTNode *analisar_atribuicao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na atribuição");
        sincronizar_parser(parser);
        return NULL;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);

    ASTNode *value  = analisar_expressao(parser);
    ASTNode *assign = ast_novo_no(AST_ASSIGNMENT, line, column);
    assign->name    = name;
    assign->left    = value;

    // Critério "declared".
    Symbol *sym = buscar_simbolo(parser, name);
    if(!sym){
        char msg[256];
        sprintf(msg, "Atribuição a variável '%s' não declarada", name);
        erro_semantico(parser, msg, line, column);
    } else {
        // Critério "initialized": agora está.
        sym->initialized = 1;
        // Critério "types".
        if(value){
            TokenType expr_type = inferir_tipo(parser, value);
            validar_tipos(parser, name, sym->type, expr_type, line, column);
        }
    }

    if(!value){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
        consumir_token(parser, TOKEN_SEMICOLON);
        return assign;
    }
    consumir_token(parser, TOKEN_SEMICOLON);
    return assign;
}

//lógica interna do for
ASTNode *analisar_atribuicao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na atribuição");
        return NULL;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);

    ASTNode *value  = analisar_expressao(parser);
    ASTNode *assign = ast_novo_no(AST_ASSIGNMENT, line, column);
    assign->name    = name;
    assign->left    = value;

    Symbol *sym = buscar_simbolo(parser, name);
    if(!sym){
        char msg[256];
        sprintf(msg, "Atribuição a variável '%s' não declarada", name);
        erro_semantico(parser, msg, line, column);
    } else {
        sym->initialized = 1;
        if(value){
            TokenType expr_type = inferir_tipo(parser, value);
            validar_tipos(parser, name, sym->type, expr_type, line, column);
        }
    }
    return assign;
}

// =============================================================================
//  COMANDO INICIADO POR IDENTIFICADOR
//  Distingue atribuição, chamada de função e pos-incremento/decremento.
//  Semântica integrada diretamente em cada ramo.
// =============================================================================

ASTNode *analisar_comando_iniciado_por_id(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no comando");
        sincronizar_parser(parser);
        return NULL;
    }

    char *name = strdup(parser->current_token.lexema);
    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    consumir_token(parser, TOKEN_ID);

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        ASTNode *value  = analisar_expressao(parser);
        ASTNode *assign = ast_novo_no(AST_ASSIGNMENT, line, column);
        assign->name    = name;
        assign->left    = value;

        Symbol *sym = buscar_simbolo(parser, name);
        if(!sym){
            char msg[256];
            sprintf(msg, "Atribuição a variável '%s' não declarada", name);
            erro_semantico(parser, msg, line, column);
        } else {
            sym->initialized = 1;
            if(value){
                TokenType expr_type = inferir_tipo(parser, value);
                validar_tipos(parser, name, sym->type, expr_type, line, column);
            }
        }

        if(!value){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return assign;
        }
        consumir_token(parser, TOKEN_SEMICOLON);
        return assign;

    // Ramo: chamada de função (id(...);)
    } else if(parser->current_token.type == TOKEN_LPAREN){
        ASTNode *call = analisar_chamada_funcao(parser, name, line, column);
        if(!call){
            free(name);
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return NULL;
        }
        consumir_token(parser, TOKEN_SEMICOLON);
        return call;

    // Ramo: pós-incremento / pós-decremento (id++;  id--;)
    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        TokenType op  = parser->current_token.type;
        int op_line   = parser->current_token.line;
        int op_column = parser->current_token.column;
        consumir_token(parser, op);

        // Critérios "declared", "initialized", "used".
        Symbol *sym = buscar_simbolo(parser, name);
        if(!sym){
            char msg[256];
            sprintf(msg, "Variável '%s' não declarada", name);
            erro_semantico(parser, msg, line, column);
        } else {
            sym->initialized = 1;
            sym->used        = 1;
        }

        ASTNode *operand    = ast_novo_identificador(name, line, column);
        ASTNode *node       = ast_novo_no(AST_UNARY_EXPR, op_line, op_column);
        node->value_type    = op;
        node->left          = operand;
        consumir_token(parser, TOKEN_SEMICOLON);
        return node;

    } else {
        erro_de_sintaxe(parser,
            "Esperado atribuição, chamada de função, ++ ou -- após o identificador");
        sincronizar_parser(parser);
        free(name);
        return NULL;
    }
}

// =============================================================================
//  COMANDOS COMPOSTOS
// =============================================================================

ASTNode *analisar_comando(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(token_eh_tipo(parser->current_token.type))
        return analisar_declaracao(parser);

    if(parser->current_token.type == TOKEN_ID)
        return analisar_comando_iniciado_por_id(parser);

    if(parser->current_token.type == TOKEN_LBRACE)
        return analisar_bloco(parser);

    if(parser->current_token.type == TOKEN_IF)
        return analisar_if(parser);

    if(parser->current_token.type == TOKEN_WHILE)
        return analisar_while(parser);

    if(parser->current_token.type == TOKEN_FOR)
        return analisar_for(parser);

    if(parser->current_token.type == TOKEN_RETURN)
        return analisar_return(parser);

    if(parser->current_token.type == TOKEN_BREAK)
        return analisar_break(parser);

    if(parser->current_token.type == TOKEN_CONTINUE)
        return analisar_continue(parser);

    erro_de_sintaxe(parser, "Comando inválido ou não suportado");
    sincronizar_parser(parser);
    return NULL;
}

ASTNode *analisar_lista_de_comandos(Parser *parser){
    ASTNode *list = NULL;

    while(parser->current_token.type != TOKEN_RBRACE &&
          parser->current_token.type != TOKEN_EOF){
        ASTNode *cmd = analisar_comando(parser);
        if(cmd) list = ast_anexar(list, cmd);
        if(parser->em_recuperacao) sincronizar_parser(parser);
    }
    return list;
}

// =============================================================================
//  BLOCO
//  Abre e fecha um novo escopo — qualquer variável declarada aqui
//  é destruída ao sair, e variáveis não usadas geram aviso.
// =============================================================================

ASTNode *analisar_bloco(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line   = parser->current_token.line;
    int column = parser->current_token.column;

    if(!consumir_token(parser, TOKEN_LBRACE)){
        sincronizar_parser(parser);
        return NULL;
    }

    entrar_escopo(parser);
    ASTNode *block = ast_novo_no(AST_BLOCK, line, column);
    block->left    = analisar_lista_de_comandos(parser);
    consumir_token(parser, TOKEN_RBRACE);
    sair_escopo(parser);   // avisa variáveis não usadas e descarta o escopo
    return block;
}

// =============================================================================
//  PARÂMETROS
//  Parâmetros são declarados no escopo da função e considerados inicializados
//  (o chamador fornece o valor).
// =============================================================================

ASTNode *analisar_parametro(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_ate(parser, TOKEN_RPAREN);
        return NULL;
    }
    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no parâmetro");
        sincronizar_ate(parser, TOKEN_RPAREN);
        return NULL;
    }

    ASTNode *param    = ast_novo_no(AST_PARAMETER,
                                     parser->current_token.line,
                                     parser->current_token.column);
    param->name       = strdup(parser->current_token.lexema);
    param->value_type = declared_type;
    consumir_token(parser, TOKEN_ID);

    // Parâmetros são "initialized = 1" por definição.
    declarar_variavel(parser, param->name, declared_type,
                      1, param->line, param->column);
    return param;
}

ASTNode *analisar_lista_de_parametros(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *list = analisar_parametro(parser);
    if(!list) return NULL;

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);
        ASTNode *param = analisar_parametro(parser);
        if(param) list = ast_anexar(list, param);
    }
    return list;
}

// =============================================================================
//  ESTRUTURAS DE CONTROLE
// =============================================================================

ASTNode *analisar_if(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line   = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_IF);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return NULL;
    }

    ASTNode *condition = analisar_condicao(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    consumir_token(parser, TOKEN_RPAREN);

    ASTNode *then_branch = analisar_comando(parser);
    ASTNode *else_branch = NULL;

    if(parser->current_token.type == TOKEN_ELSE){
        consumir_token(parser, TOKEN_ELSE);
        else_branch = analisar_comando(parser);
    }

    ASTNode *node  = ast_novo_no(AST_IF, line, column);
    node->left     = condition;
    node->right    = then_branch;
    node->third    = else_branch;
    return node;
}

ASTNode *analisar_while(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line   = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_WHILE);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return NULL;
    }

    ASTNode *condition = analisar_condicao(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    consumir_token(parser, TOKEN_RPAREN);

    ASTNode *body  = analisar_comando(parser);
    ASTNode *node  = ast_novo_no(AST_WHILE, line, column);
    node->left     = condition;
    node->right    = body;
    return node;
}

// =============================================================================
//  FOR
//  A inicialização abre escopo antes do bloco para que a variável do for
//  (ex.: int i = 0) seja visível na condição e no incremento, mas não
//  vaze para fora do for.
// =============================================================================

ASTNode *analisar_inicializacao_for(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(token_eh_tipo(parser->current_token.type))
        return analisar_declaracao_sem_ponto_virgula(parser);

    if(parser->current_token.type == TOKEN_ID)
        return analisar_atribuicao_sem_ponto_virgula(parser);

    erro_de_sintaxe(parser, "Inicialização inválida no for");
    return NULL;
}

ASTNode *analisar_expressao_de_incremento(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    // pós-incremento/decremento: i++  i--
    if(parser->current_token.type == TOKEN_ID){
        int   line   = parser->current_token.line;
        int   column = parser->current_token.column;
        char *name   = strdup(parser->current_token.lexema);
        consumir_token(parser, TOKEN_ID);

        if(parser->current_token.type == TOKEN_INCREMENT ||
           parser->current_token.type == TOKEN_DECREMENT){
            TokenType op = parser->current_token.type;
            consumir_token(parser, op);

            Symbol *sym = buscar_simbolo(parser, name);
            if(!sym){
                char msg[256];
                sprintf(msg, "Variável '%s' não declarada", name);
                erro_semantico(parser, msg, line, column);
            } else {
                sym->initialized = 1;
                sym->used        = 1;
            }

            ASTNode *operand    = ast_novo_identificador(name, line, column);
            free(name);
            ASTNode *node       = ast_novo_no(AST_UNARY_EXPR, line, column);
            node->value_type    = op;
            node->left          = operand;
            return node;

        } else if(parser->current_token.type == TOKEN_ASSIGN){
            consumir_token(parser, TOKEN_ASSIGN);
            ASTNode *expr   = analisar_expressao(parser);
            ASTNode *assign = ast_novo_no(AST_ASSIGNMENT, line, column);
            assign->name    = name;
            assign->left    = expr;

            Symbol *sym = buscar_simbolo(parser, name);
            if(!sym){
                char msg[256];
                sprintf(msg, "Variável '%s' não declarada", name);
                erro_semantico(parser, msg, line, column);
            } else {
                sym->initialized = 1;
                if(expr){
                    TokenType expr_type = inferir_tipo(parser, expr);
                    validar_tipos(parser, name, sym->type, expr_type, line, column);
                }
            }
            return assign;
        }

        free(name);
        erro_de_sintaxe(parser, "Esperado ++, -- ou = na parte de incremento do for");
        return NULL;

    // pré-incremento/decremento: ++i  --i
    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        TokenType op  = parser->current_token.type;
        int line      = parser->current_token.line;
        int column    = parser->current_token.column;
        consumir_token(parser, op);

        if(parser->current_token.type != TOKEN_ID){
            erro_de_sintaxe(parser, "Esperado identificador após ++/--");
            return NULL;
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

        ASTNode *operand    = ast_novo_identificador(name, id_line, id_column);
        consumir_token(parser, TOKEN_ID);
        ASTNode *node       = ast_novo_no(AST_UNARY_EXPR, line, column);
        node->value_type    = op;
        node->left          = operand;
        return node;

    } else {
        return NULL;
    }
}

ASTNode *analisar_for(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line   = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_FOR);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return NULL;
    }

    // Escopo do for: cobre init, condição, incremento e corpo.
    entrar_escopo(parser);

    ASTNode *init = analisar_inicializacao_for(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_SEMICOLON);
    consumir_token(parser, TOKEN_SEMICOLON);

    ASTNode *condition = analisar_condicao(parser);
    if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_SEMICOLON);
    consumir_token(parser, TOKEN_SEMICOLON);

    ASTNode *increment = analisar_expressao_de_incremento(parser);
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
                return NULL;
            }
        }
    }

    ASTNode *body  = analisar_comando(parser);
    sair_escopo(parser);

    ASTNode *node  = ast_novo_no(AST_FOR, line, column);
    node->left     = init;
    node->right    = condition;
    node->third    = increment;
    node->extra    = body;
    return node;
}

ASTNode *analisar_return(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line   = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_RETURN);

    ASTNode *node = ast_novo_no(AST_RETURN, line, column);

    if(parser->current_token.type != TOKEN_SEMICOLON){
        node->left = analisar_expressao(parser);
        if(!node->left){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
        } else {
            // Valida o tipo do valor retornado.
            TokenType expr_type = inferir_tipo(parser, node->left);
            TokenType ret_type  = parser->current_return_type;

            if(ret_type == TOKEN_VOID){
                char msg[256];
                sprintf(msg,
                    "Função void não pode retornar um valor (linha %d)", line);
                erro_semantico(parser, msg, line, column);
            } else if(expr_type != TOKEN_ERROR){
                // Reutiliza validar_tipos com nome fictício para a mensagem.
                validar_tipos(parser, "<retorno>", ret_type, expr_type, line, column);
            }
        }
    } else {
        // return; sem valor — só é válido em void.
        if(parser->current_return_type != TOKEN_VOID &&
           parser->current_return_type != TOKEN_ERROR){
            char msg[256];
            sprintf(msg,
                "Função não-void deve retornar um valor");
            erro_semantico(parser, msg, line, column);
        }
    }

    consumir_token(parser, TOKEN_SEMICOLON);
    return node;
}

ASTNode *analisar_break(Parser *parser){
    if(parser->em_recuperacao) return NULL;
    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_BREAK);
    consumir_token(parser, TOKEN_SEMICOLON);
    return ast_novo_no(AST_BREAK, line, column);
}

ASTNode *analisar_continue(Parser *parser){
    if(parser->em_recuperacao) return NULL;
    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_CONTINUE);
    consumir_token(parser, TOKEN_SEMICOLON);
    return ast_novo_no(AST_CONTINUE, line, column);
}

// =============================================================================
//  FUNÇÕES
//  analisar_funcao abre o escopo antes dos parâmetros para que eles fiquem
//  visíveis no corpo. O bloco interno abrirá seu próprio sub-escopo.
// =============================================================================

ASTNode *analisar_funcao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType return_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return NULL;
    }
    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no nome da função");
        sincronizar_parser(parser);
        return NULL;
    }

    int   line   = parser->current_token.line;
    int   column = parser->current_token.column;
    char *name   = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        free(name);
        return NULL;
    }

    // Escopo da função: parâmetros vivem aqui.
    entrar_escopo(parser);

    ASTNode *params = NULL;
    if(parser->current_token.type != TOKEN_RPAREN){
        params = analisar_lista_de_parametros(parser);
        if(parser->em_recuperacao) sincronizar_ate(parser, TOKEN_RPAREN);
    }

    // Salva o tipo de retorno para que analisar_return possa validar.
    TokenType tipo_retorno_anterior = parser->current_return_type;
    parser->current_return_type = return_type;

    consumir_token(parser, TOKEN_RPAREN);
    ASTNode *body = analisar_bloco(parser); // bloco cria sub-escopo próprio
    sair_escopo(parser);                    // fecha escopo da função

    // Restaura (suporte a funções aninhadas ou futuras extensões).
    parser->current_return_type = tipo_retorno_anterior;

    ASTNode *func    = ast_novo_no(AST_FUNCTION_DECL, line, column);
    func->name       = name;
    func->value_type = return_type;
    func->left       = params;
    func->right      = body;
    return func;
}

ASTNode *analisar_lista_de_funcoes(Parser *parser){
    ASTNode *functions = NULL;

    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == TOKEN_RBRACE){
            erro_de_sintaxe(parser,
                "Chave de fechamento inesperada no escopo global");
            avancar_token(parser);
            parser->em_recuperacao = 0;
            continue;
        }

        ASTNode *func = analisar_funcao(parser);
        if(func) functions = ast_anexar(functions, func);

        if(parser->em_recuperacao){
            sincronizar_parser(parser);
            if(parser->current_token.type == TOKEN_RBRACE){
                avancar_token(parser);
                parser->em_recuperacao = 0;
            }
        }
    }
    return functions;
}

ASTNode *analisar_programa(Parser *parser){
    ASTNode *root = ast_novo_no(AST_PROGRAM,
                                 parser->current_token.line,
                                 parser->current_token.column);
    root->left = analisar_lista_de_funcoes(parser);
    consumir_token(parser, TOKEN_EOF);
    return root;
}