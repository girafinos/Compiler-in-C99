#include "parser.h"

// =============================================================================
//  INFRAESTRUTURA DO PARSER
// =============================================================================

void inicializar_parser(Parser *parser, Lexer *lexer){
    parser->lexer          = lexer;
    parser->current_token  = pegar_prox_token(lexer);
    parser->em_recuperacao = 0;
    parser->quantidade_erros = 0;
}

void avancar_token(Parser *parser){
    parser->current_token = pegar_prox_token(parser->lexer);
}

// -----------------------------------------------------------------------------
// erro_de_sintaxe — agora retorna int (sempre 0 / falha).
// Permite escrever:  return erro_de_sintaxe(parser, "...");
// ou:               if(!analisar_X(parser)) { ... }
// -----------------------------------------------------------------------------
int erro_de_sintaxe(Parser *parser, const char *mensagem){
    // Se já estamos em recuperação, silencia para não gerar cascata.
    if(parser->em_recuperacao) return 0;

    parser->em_recuperacao = 1;
    parser->quantidade_erros++;
    printf("\n");

    printf(RED
           "[ ERRO SINTÁTICO #%d ]\n"
           RESET,
           parser->quantidade_erros);

    printf(YELLOW "Mensagem:\n" RESET);
    printf("  %s\n\n", mensagem);

    printf(YELLOW "Localização:\n" RESET);
    printf("  Linha: %d\n", parser->current_token.line);
    printf("  Coluna: %d\n\n", parser->current_token.column);

    printf(YELLOW "Token encontrado:\n" RESET);
    printf("  Tipo: %s\n", token_para_string(parser->current_token.type));
    printf("  Símbolo: %s\n", token_para_simbolo(parser->current_token.type));
    printf("  Lexema: \"%s\"\n\n", parser->current_token.lexema);

    printf(YELLOW "Trecho:\n" RESET);
    mostrar_linha_erro(parser);

    return 0; // sempre indica falha
}

// -----------------------------------------------------------------------------
// consumir_token — retorna 1 (sucesso) ou 0 (falha).
// Se já estamos em recuperação, retorna 0 sem gerar novo erro.
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// sincronizar_parser — recuperação genérica: avança até ';' ou '}'.
// Usado em comandos quando não sabemos um token de retomada mais preciso.
// -----------------------------------------------------------------------------
void sincronizar_parser(Parser *parser){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == TOKEN_SEMICOLON){
            avancar_token(parser); // consome o ';'
            break;
        }
        if(parser->current_token.type == TOKEN_RBRACE){
            break; // não consome o '}': pertence ao bloco pai
        }
        avancar_token(parser);
    }
    parser->em_recuperacao = 0; // pronto para retomar
}

// -----------------------------------------------------------------------------
// sincronizar_ate — recuperação cirúrgica: avança até um token específico
// e o deixa como current_token (sem consumir), liberando o chamador para
// fazer consumir_token normalmente em seguida.
// -----------------------------------------------------------------------------
void sincronizar_ate(Parser *parser, TokenType token){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == token){
            parser->em_recuperacao = 0; // pronto para retomar
            return;
        }
        avancar_token(parser);
    }
    // Se chegou no EOF sem achar o token, apenas libera o flag.
    parser->em_recuperacao = 0;
}

// =============================================================================
//  UTILITÁRIOS DE EXIBIÇÃO
// =============================================================================

void mostrar_linha_erro(Parser *parser){
    const char *source = parser->lexer->src;
    int linha_atual = 1;
    int i = 0;

    while(source[i] != '\0'){
        if(linha_atual == parser->current_token.line){
            printf(CYAN "%4d | " RESET, linha_atual);

            while(source[i] != '\n' && source[i] != '\0'){
                printf("%c", source[i]);
                i++;
            }

            printf("\n       ");

            for(int j = 1; j < parser->current_token.column; j++){
                printf(" ");
            }

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
        case TOKEN_EOF:          return "EOF";
        default:                 return "desconhecido";
    }
}

// =============================================================================
//  TIPOS E OPERADORES — AUXILIARES
// =============================================================================

int token_eh_tipo(TokenType type){
    return type == TOKEN_INT  ||
           type == TOKEN_CHAR ||
           type == TOKEN_VOID;
}

int token_eh_operador_relacional(TokenType type){
    return type == TOKEN_LT  ||
           type == TOKEN_GT  ||
           type == TOKEN_LTE ||
           type == TOKEN_GTE ||
           type == TOKEN_EQ  ||
           type == TOKEN_NEQ;
}

// analisar_tipo agora retorna int para que o chamador saiba se falhou.
int analisar_tipo(Parser *parser){
    if(parser->em_recuperacao) return 0;

    if(token_eh_tipo(parser->current_token.type)){
        avancar_token(parser);
        return 1;
    }

    return erro_de_sintaxe(parser, "Tipo esperado: int, char ou void");
}

// =============================================================================
//  EXPRESSÕES
//  Todas retornam int: 1 = ok, 0 = erro.
//  Cada nível para imediatamente se o nível interno falhou.
// =============================================================================

// -----------------------------------------------------------------------------
// analisar_lista_de_argumentos
// -----------------------------------------------------------------------------
int analisar_lista_de_argumentos(Parser *parser){
    if(parser->em_recuperacao) return 0;

    if(!analisar_expressao(parser)) return 0;

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);

        if(parser->current_token.type == TOKEN_RPAREN){
            return erro_de_sintaxe(parser,
                       "Falta uma expressão após a vírgula");
        }

        if(!analisar_expressao(parser)) return 0;
    }

    return 1;
}

// -----------------------------------------------------------------------------
// analisar_fator — nível mais profundo da hierarquia de expressões.
// -----------------------------------------------------------------------------
int analisar_fator(Parser *parser){
    if(parser->em_recuperacao) return 0;

    // Operador unário: negativo
    if(parser->current_token.type == TOKEN_MINUS){
        consumir_token(parser, TOKEN_MINUS);
        return analisar_fator(parser);
    }

    // Operador unário: negação lógica
    if(parser->current_token.type == TOKEN_NOT){
        consumir_token(parser, TOKEN_NOT);
        return analisar_fator(parser);
    }

    // Identificador ou chamada de função
    if(parser->current_token.type == TOKEN_ID){
        consumir_token(parser, TOKEN_ID);

        if(parser->current_token.type == TOKEN_LPAREN){
            // chamada de função
            consumir_token(parser, TOKEN_LPAREN);

            if(parser->current_token.type != TOKEN_RPAREN){
                if(!analisar_lista_de_argumentos(parser)){
                    sincronizar_ate(parser, TOKEN_RPAREN);
                }
            }

            return consumir_token(parser, TOKEN_RPAREN);
        }

        return 1;
    }

    // Número literal
    if(parser->current_token.type == TOKEN_NUM){
        return consumir_token(parser, TOKEN_NUM);
    }

    // Char literal
    if(parser->current_token.type == TOKEN_CHAR_LITERAL){
        return consumir_token(parser, TOKEN_CHAR_LITERAL);
    }

    // String literal
    if(parser->current_token.type == TOKEN_STRING){
        return consumir_token(parser, TOKEN_STRING);
    }

    // Expressão entre parênteses
    if(parser->current_token.type == TOKEN_LPAREN){
        consumir_token(parser, TOKEN_LPAREN);

        if(parser->current_token.type == TOKEN_RPAREN){
            return erro_de_sintaxe(parser,
                       "Expressão vazia entre parênteses");
        }

        if(!analisar_expressao(parser)){
            sincronizar_ate(parser, TOKEN_RPAREN);
        }

        return consumir_token(parser, TOKEN_RPAREN);
    }

    return erro_de_sintaxe(parser, "Expressão inválida: fator ausente ou operador isolado");
}

// -----------------------------------------------------------------------------
// analisar_termo — multiplicação e divisão
// -----------------------------------------------------------------------------
int analisar_termo(Parser *parser){
    if(parser->em_recuperacao) return 0;

    if(!analisar_fator(parser)) return 0;

    while(parser->current_token.type == TOKEN_MULT ||
          parser->current_token.type == TOKEN_DIV){

        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        if(!analisar_fator(parser)) return 0;
    }

    return 1;
}

// -----------------------------------------------------------------------------
// analisar_expressao — adição e subtração
// -----------------------------------------------------------------------------
int analisar_expressao(Parser *parser){
    if(parser->em_recuperacao) return 0;

    if(!analisar_termo(parser)) return 0;

    while(parser->current_token.type == TOKEN_PLUS ||
          parser->current_token.type == TOKEN_MINUS){

        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        if(!analisar_termo(parser)) return 0;
    }

    return 1;
}

// =============================================================================
//  CONDIÇÕES
//  Também retornam int para que if/while/for possam recuperar corretamente.
// =============================================================================

void analisar_operador_relacional(Parser *parser){
    if(token_eh_operador_relacional(parser->current_token.type)){
        avancar_token(parser);
    } else {
        erro_de_sintaxe(parser, "Operador relacional ausente na condição");
    }
}

int analisar_condicao_relacional(Parser *parser){
    if(parser->em_recuperacao) return 0;

    if(!analisar_expressao(parser)) return 0;

    if(token_eh_operador_relacional(parser->current_token.type)){
        analisar_operador_relacional(parser);
        return analisar_expressao(parser);
    }

    return 1;
}

int analisar_condicao(Parser *parser){
    if(parser->em_recuperacao) return 0;

    if(!analisar_condicao_relacional(parser)) return 0;

    while(parser->current_token.type == TOKEN_AND ||
          parser->current_token.type == TOKEN_OR){

        TokenType op = parser->current_token.type;
        consumir_token(parser, op);

        if(!analisar_condicao_relacional(parser)) return 0;
    }

    return 1;
}

// =============================================================================
//  COMANDOS — void porque são retomados pelo loop de analisar_lista_de_comandos
// =============================================================================

// -----------------------------------------------------------------------------
// analisar_declaracao
// -----------------------------------------------------------------------------
void analisar_declaracao(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return;
    }

    if(!consumir_token(parser, TOKEN_ID)){
        sincronizar_parser(parser);
        return;
    }

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);

        if(!analisar_expressao(parser)){
            // Expressão inválida: vai até o ';' para retomar no próximo comando
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return;
        }
    }

    if(!consumir_token(parser, TOKEN_SEMICOLON)){
        parser->em_recuperacao = 0;
        return;
    }
}

// -----------------------------------------------------------------------------
// analisar_declaracao_sem_ponto_virgula  (usada dentro do for)
// -----------------------------------------------------------------------------
void analisar_declaracao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!analisar_tipo(parser)) return;

    if(!consumir_token(parser, TOKEN_ID)) return;

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        analisar_expressao(parser);
    }
}

// -----------------------------------------------------------------------------
// analisar_atribuicao
// -----------------------------------------------------------------------------
void analisar_atribuicao(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!consumir_token(parser, TOKEN_ID))  { sincronizar_parser(parser); return; }
    if(!consumir_token(parser, TOKEN_ASSIGN)){ sincronizar_parser(parser); return; }

    if(!analisar_expressao(parser)){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
        consumir_token(parser, TOKEN_SEMICOLON);
        return;
    }

    consumir_token(parser, TOKEN_SEMICOLON);
}

// -----------------------------------------------------------------------------
// analisar_atribuicao_sem_ponto_virgula  (usada dentro do for)
// -----------------------------------------------------------------------------
void analisar_atribuicao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!consumir_token(parser, TOKEN_ID))   return;
    if(!consumir_token(parser, TOKEN_ASSIGN)) return;

    analisar_expressao(parser);
}

// -----------------------------------------------------------------------------
// analisar_if
// -----------------------------------------------------------------------------
void analisar_if(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_IF);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return;
    }

    if(!analisar_condicao(parser)){
        sincronizar_ate(parser, TOKEN_RPAREN);
    }

    if(!consumir_token(parser, TOKEN_RPAREN)){
        if(parser->current_token.type == TOKEN_LBRACE){
            parser->em_recuperacao = 0;
        } else {
            sincronizar_ate(parser, TOKEN_RPAREN);
            if(parser->current_token.type == TOKEN_RPAREN){
                consumir_token(parser, TOKEN_RPAREN);
            } else {
                sincronizar_parser(parser);
                return;
            }
        }
    }

    analisar_comando(parser);

    if(parser->current_token.type == TOKEN_ELSE){
        consumir_token(parser, TOKEN_ELSE);
        analisar_comando(parser);
    }
}

// -----------------------------------------------------------------------------
// analisar_while
// -----------------------------------------------------------------------------
void analisar_while(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_WHILE);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return;
    }

    if(!analisar_condicao(parser)){
        sincronizar_ate(parser, TOKEN_RPAREN);
    }

    if(!consumir_token(parser, TOKEN_RPAREN)){
        if(parser->current_token.type == TOKEN_LBRACE){
            parser->em_recuperacao = 0;
        } else {
            sincronizar_ate(parser, TOKEN_RPAREN);
            if(parser->current_token.type == TOKEN_RPAREN){
                consumir_token(parser, TOKEN_RPAREN);
            } else {
                sincronizar_parser(parser);
                return;
            }
        }
    }

    analisar_comando(parser);
}

// -----------------------------------------------------------------------------
// analisar_inicializacao_for
// -----------------------------------------------------------------------------
void analisar_inicializacao_for(Parser *parser){
    if(parser->em_recuperacao) return;

    if(token_eh_tipo(parser->current_token.type)){
        analisar_declaracao_sem_ponto_virgula(parser);

    } else if(parser->current_token.type == TOKEN_ID){
        analisar_atribuicao_sem_ponto_virgula(parser);

    } else {
        erro_de_sintaxe(parser, "Inicialização inválida no for");
    }
}

// -----------------------------------------------------------------------------
// analisar_expressao_de_incremento  (terceiro campo do for)
// -----------------------------------------------------------------------------
void analisar_expressao_de_incremento(Parser *parser){
    if(parser->em_recuperacao) return;

    if(parser->current_token.type == TOKEN_ID){
        consumir_token(parser, TOKEN_ID);

        if(parser->current_token.type == TOKEN_INCREMENT){
            consumir_token(parser, TOKEN_INCREMENT);

        } else if(parser->current_token.type == TOKEN_DECREMENT){
            consumir_token(parser, TOKEN_DECREMENT);

        } else if(parser->current_token.type == TOKEN_ASSIGN){
            consumir_token(parser, TOKEN_ASSIGN);
            analisar_expressao(parser);

        } else {
            erro_de_sintaxe(parser,
                "Esperado ++, -- ou = na parte de incremento do for");
        }

    } else if(parser->current_token.type == TOKEN_INCREMENT){
        consumir_token(parser, TOKEN_INCREMENT);
        consumir_token(parser, TOKEN_ID);

    } else if(parser->current_token.type == TOKEN_DECREMENT){
        consumir_token(parser, TOKEN_DECREMENT);
        consumir_token(parser, TOKEN_ID);

    } else {
        erro_de_sintaxe(parser, "Parte de incremento do for inválida");
    }
}

// -----------------------------------------------------------------------------
// analisar_for
// -----------------------------------------------------------------------------
void analisar_for(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_FOR);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return;
    }

    // Parte 1: inicialização
    analisar_inicializacao_for(parser);
    if(parser->em_recuperacao){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
    }
    consumir_token(parser, TOKEN_SEMICOLON);

    // Parte 2: condição
    if(!analisar_condicao(parser)){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
    }
    consumir_token(parser, TOKEN_SEMICOLON);

    // Parte 3: incremento
    analisar_expressao_de_incremento(parser);
    if(parser->em_recuperacao){
        sincronizar_ate(parser, TOKEN_RPAREN);
    }

    if(!consumir_token(parser, TOKEN_RPAREN)){
        if(parser->current_token.type == TOKEN_LBRACE){
            parser->em_recuperacao = 0;
        } else {
            sincronizar_ate(parser, TOKEN_RPAREN);
            if(parser->current_token.type == TOKEN_RPAREN){
                consumir_token(parser, TOKEN_RPAREN);
            } else {
                sincronizar_parser(parser);
                return;
            }
        }
    }

    analisar_comando(parser);
}

// -----------------------------------------------------------------------------
// analisar_return
// -----------------------------------------------------------------------------
void analisar_return(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_RETURN);

    if(parser->current_token.type != TOKEN_SEMICOLON){
        if(!analisar_expressao(parser)){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return;
        }
    }

    consumir_token(parser, TOKEN_SEMICOLON);
}

// -----------------------------------------------------------------------------
// analisar_break / analisar_continue
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// analisar_incremento_decremento  (prefixo: ++x; --x;)
// -----------------------------------------------------------------------------
void analisar_incremento_decremento(Parser *parser){
    if(parser->em_recuperacao) return;

    if(parser->current_token.type == TOKEN_INCREMENT){
        consumir_token(parser, TOKEN_INCREMENT);
        consumir_token(parser, TOKEN_ID);
        consumir_token(parser, TOKEN_SEMICOLON);

    } else if(parser->current_token.type == TOKEN_DECREMENT){
        consumir_token(parser, TOKEN_DECREMENT);
        consumir_token(parser, TOKEN_ID);
        consumir_token(parser, TOKEN_SEMICOLON);

    } else {
        erro_de_sintaxe(parser, "Esperado ++ ou -- no início do comando");
    }
}

// -----------------------------------------------------------------------------
// analisar_comando_iniciado_por_id  (atribuição, chamada, x++, x--)
// -----------------------------------------------------------------------------
void analisar_comando_iniciado_por_id(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_ID);

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);

        if(!analisar_expressao(parser)){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return;
        }

        consumir_token(parser, TOKEN_SEMICOLON);

    } else if(parser->current_token.type == TOKEN_LPAREN){
        consumir_token(parser, TOKEN_LPAREN);

        if(parser->current_token.type != TOKEN_RPAREN){
            if(!analisar_lista_de_argumentos(parser)){
                sincronizar_ate(parser, TOKEN_RPAREN);
            }
        }

        consumir_token(parser, TOKEN_RPAREN);
        consumir_token(parser, TOKEN_SEMICOLON);

    } else if(parser->current_token.type == TOKEN_INCREMENT){
        consumir_token(parser, TOKEN_INCREMENT);
        consumir_token(parser, TOKEN_SEMICOLON);

    } else if(parser->current_token.type == TOKEN_DECREMENT){
        consumir_token(parser, TOKEN_DECREMENT);
        consumir_token(parser, TOKEN_SEMICOLON);

    } else {
        erro_de_sintaxe(parser,
            "Esperado atribuição, chamada de função, ++ ou -- após o identificador");
        sincronizar_parser(parser);
    }
}

// -----------------------------------------------------------------------------
// analisar_comando — dispatcher central de comandos
// -----------------------------------------------------------------------------
void analisar_comando(Parser *parser){
    if(parser->em_recuperacao) return;

    if(token_eh_tipo(parser->current_token.type)){
        analisar_declaracao(parser);

    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        analisar_incremento_decremento(parser);

    } else if(parser->current_token.type == TOKEN_ID){
        analisar_comando_iniciado_por_id(parser);

    } else if(parser->current_token.type == TOKEN_LBRACE){
        analisar_bloco(parser);

    } else if(parser->current_token.type == TOKEN_IF){
        analisar_if(parser);

    } else if(parser->current_token.type == TOKEN_WHILE){
        analisar_while(parser);

    } else if(parser->current_token.type == TOKEN_FOR){
        analisar_for(parser);

    } else if(parser->current_token.type == TOKEN_RETURN){
        analisar_return(parser);

    } else if(parser->current_token.type == TOKEN_BREAK){
        analisar_break(parser);

    } else if(parser->current_token.type == TOKEN_CONTINUE){
        analisar_continue(parser);

    } else {
        erro_de_sintaxe(parser, "Comando inválido ou não suportado");
        sincronizar_parser(parser);
    }
}

// =============================================================================
//  BLOCOS E FUNÇÕES
// =============================================================================

// -----------------------------------------------------------------------------
// analisar_lista_de_comandos
// O fallback de sincronização aqui garante que, se alguma função de comando
// esqueceu de sincronizar antes de retornar, o loop não trava.
// -----------------------------------------------------------------------------
void analisar_lista_de_comandos(Parser *parser){
    while(parser->current_token.type != TOKEN_RBRACE &&
          parser->current_token.type != TOKEN_EOF){

        analisar_comando(parser);

        // Fallback: se ainda estamos em recuperação após o comando,
        // sincroniza aqui para não travar o loop.
        if(parser->em_recuperacao){
            sincronizar_parser(parser);
        }
    }
}

void analisar_bloco(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!consumir_token(parser, TOKEN_LBRACE)){
        sincronizar_parser(parser);
        return;
    }

    analisar_lista_de_comandos(parser);

    consumir_token(parser, TOKEN_RBRACE);
}

void analisar_parametro(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!analisar_tipo(parser)){
        sincronizar_ate(parser, TOKEN_RPAREN);
        return;
    }

    consumir_token(parser, TOKEN_ID);
}

void analisar_lista_de_parametros(Parser *parser){
    if(parser->em_recuperacao) return;

    analisar_parametro(parser);

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);
        analisar_parametro(parser);
    }
}

void analisar_funcao(Parser *parser){
    if(parser->em_recuperacao) return;

    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return;
    }

    if(!consumir_token(parser, TOKEN_ID)){
        sincronizar_parser(parser);
        return;
    }

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return;
    }

    if(parser->current_token.type != TOKEN_RPAREN){
        analisar_lista_de_parametros(parser);
        if(parser->em_recuperacao){
            sincronizar_ate(parser, TOKEN_RPAREN);
        }
    }

    if(!consumir_token(parser, TOKEN_RPAREN)){
        sincronizar_ate(parser, TOKEN_LBRACE);
    }

    analisar_bloco(parser);
}

// -----------------------------------------------------------------------------
// analisar_lista_de_funcoes
// Mesmo fallback do loop de comandos: garante retomada após erro em função.
//
// ATENÇÃO — loop infinito sem a guarda de TOKEN_RBRACE:
//   sincronizar_parser para em '}' sem consumir (correto dentro de blocos).
//   No escopo global não há bloco pai aguardando esse '}', então o loop
//   chamaria analisar_funcao indefinidamente sobre o mesmo token.
//   Solução: qualquer '}' encontrado no escopo global é espúrio — reporta
//   o erro e descarta o token antes de continuar.
// -----------------------------------------------------------------------------
void analisar_lista_de_funcoes(Parser *parser){
    while(parser->current_token.type != TOKEN_EOF){

        // Chave de fechamento solta no escopo global — descarta e reporta.
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

            // sincronizar_parser para em '}' sem consumir.
            // No escopo global essa chave é espúria — descarta.
            if(parser->current_token.type == TOKEN_RBRACE){
                avancar_token(parser);
                parser->em_recuperacao = 0;
            }
        }
    }
}

void analisar_programa(Parser *parser){
    analisar_lista_de_funcoes(parser);
    consumir_token(parser, TOKEN_EOF);
}

// -----------------------------------------------------------------------------
// analisar_chamada_funcao — utilitário (usado quando a chamada aparece
// como expressão isolada já com o ID consumido pelo chamador)
// -----------------------------------------------------------------------------
void analisar_chamada_funcao(Parser *parser){
    if(parser->em_recuperacao) return;

    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_LPAREN);

    if(parser->current_token.type != TOKEN_RPAREN){
        if(!analisar_lista_de_argumentos(parser)){
            sincronizar_ate(parser, TOKEN_RPAREN);
        }
    }

    consumir_token(parser, TOKEN_RPAREN);
}