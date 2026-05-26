#include "parser.h"

void inicializar_parser(Parser *parser, Lexer *lexer){
    parser->lexer = lexer;
    parser->current_token = pegar_prox_token(lexer);
    parser->em_recuperacao = 0;
    parser->quantidade_erros = 0;
}

void avancar_token(Parser *parser){
    parser->current_token = pegar_prox_token(parser->lexer);
}

void erro_de_sintaxe(Parser *parser, const char *mensagem){
    if(parser->em_recuperacao){
        return;
    }

    parser->em_recuperacao = 1;
    parser->quantidade_erros++;
    printf("\n");

    printf(RED
           "[ ERRO SINTÁTICO #%d ]\n\n"
           RESET,
           parser->quantidade_erros);

    printf(YELLOW "Mensagem:\n" RESET);
    printf("  %s\n\n", mensagem);

    printf(YELLOW "Localização:\n" RESET);

    printf("  Linha  : %d\n",
           parser->current_token.line);

    printf("  Coluna : %d\n\n",
           parser->current_token.column);

    printf(YELLOW "Token encontrado:\n" RESET);

    printf("  Tipo    : %s\n",
           token_para_string(
               parser->current_token.type
           ));
    printf("  Símbolo : %s\n",
           token_para_simbolo(
               parser->current_token.type
           ));
    printf("  Lexema  : \"%s\"\n\n",
           parser->current_token.lexema);

    printf(YELLOW "Trecho:\n" RESET);
    mostrar_linha_erro(parser);
}

void consumir_token(Parser *parser, TokenType tipo_esperado){
    if(parser->current_token.type == tipo_esperado){
        avancar_token(parser);

    }else{
        char mensagem[200];
        sprintf(
            mensagem,
            "Esperado %s mas encontrado %s",
            token_para_string(tipo_esperado),
            token_para_string(parser->current_token.type)
        );

        erro_de_sintaxe(parser, mensagem);
    }
}

void sincronizar_parser(Parser *parser){
    while(parser->current_token.type != TOKEN_EOF){

        if(parser->current_token.type == TOKEN_SEMICOLON){
            avancar_token(parser);
            break;
        }

        if(parser->current_token.type == TOKEN_RBRACE){
            break;
        }

        avancar_token(parser);
    }

    parser->em_recuperacao = 0;
}

void sincronizar_ate(Parser *parser, TokenType token){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == token){
            return;
        }

        avancar_token(parser);
    }
}

void mostrar_linha_erro(Parser *parser){
    char *source = parser->lexer->src;
    int linha_atual = 1;
    int i = 0;

    while(source[i] != '\0'){
        if(linha_atual == parser->current_token.line){
            printf(CYAN "%4d | " RESET,
                   linha_atual);

            while(source[i] != '\n' &&
                  source[i] != '\0'){

                printf("%c", source[i]);
                i++;
            }

            printf("\n");
            printf("       ");

            for(int j = 1;
                j < parser->current_token.column;
                j++){

                printf(" ");
            }

            printf(RED "^\n" RESET);
            return;
        }

        if(source[i] == '\n'){
            linha_atual++;
        }

        i++;
    }
}

const char *token_para_simbolo(TokenType type){
    switch(type){

        case TOKEN_INT:         return "int";
        case TOKEN_CHAR:        return "char";
        case TOKEN_VOID:        return "void";

        case TOKEN_IF:          return "if";
        case TOKEN_ELSE:        return "else";
        case TOKEN_WHILE:       return "while";
        case TOKEN_FOR:         return "for";
        case TOKEN_RETURN:      return "return";
        case TOKEN_BREAK:       return "break";
        case TOKEN_CONTINUE:    return "continue";

        case TOKEN_PLUS:        return "+";
        case TOKEN_MINUS:       return "-";
        case TOKEN_MULT:        return "*";
        case TOKEN_DIV:         return "/";

        case TOKEN_ASSIGN:      return "=";

        case TOKEN_EQ:          return "==";
        case TOKEN_NEQ:         return "!=";

        case TOKEN_LT:          return "<";
        case TOKEN_GT:          return ">";
        case TOKEN_LTE:         return "<=";
        case TOKEN_GTE:         return ">=";

        case TOKEN_AND:         return "&&";
        case TOKEN_OR:          return "||";
        case TOKEN_NOT:         return "!";

        case TOKEN_INCREMENT:   return "++";
        case TOKEN_DECREMENT:   return "--";

        case TOKEN_LPAREN:      return "(";
        case TOKEN_RPAREN:      return ")";

        case TOKEN_LBRACE:      return "{";
        case TOKEN_RBRACE:      return "}";

        case TOKEN_COMMA:       return ",";
        case TOKEN_SEMICOLON:   return ";";

        case TOKEN_ID:          return "identificador";
        case TOKEN_NUM:         return "numero";

        case TOKEN_STRING:      return "string";
        case TOKEN_CHAR_LITERAL:return "char literal";

        case TOKEN_EOF:         return "EOF";

        default:                return "desconhecido";
    }
}

void analisar_programa(Parser *parser){
    analisar_lista_de_funcoes(parser);
    consumir_token(parser, TOKEN_EOF);
}

void analisar_lista_de_funcoes(Parser *parser){
    while (parser->current_token.type != TOKEN_EOF) {
        analisar_funcao(parser);
    }
}

void analisar_funcao(Parser *parser){
    analisar_tipo(parser);
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_LPAREN);

    if(parser->current_token.type != TOKEN_RPAREN){
        analisar_lista_de_parametros(parser);
    }

    consumir_token(parser, TOKEN_RPAREN);

    analisar_bloco(parser);
}

void analisar_lista_de_parametros(Parser *parser){
    analisar_parametro(parser);

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);
        analisar_parametro(parser);
    }
}

void analisar_parametro(Parser *parser){
    analisar_tipo(parser);
    consumir_token(parser, TOKEN_ID);
}

void analisar_bloco(Parser *parser){
    consumir_token(parser, TOKEN_LBRACE);
    analisar_lista_de_comandos(parser);
    consumir_token(parser, TOKEN_RBRACE);
}

void analisar_lista_de_comandos(Parser *parser){

    while(parser->current_token.type != TOKEN_RBRACE &&
          parser->current_token.type != TOKEN_EOF){

        analisar_comando(parser);
    }
}

void analisar_comando(Parser *parser){
    if(parser->current_token.type == TOKEN_INT ||
       parser->current_token.type == TOKEN_CHAR ||
       parser->current_token.type == TOKEN_VOID){
        analisar_declaracao(parser);

    } else if(parser->current_token.type == TOKEN_INCREMENT || parser->current_token.type == TOKEN_DECREMENT){
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

    }else{
        erro_de_sintaxe(
            parser,
            "Comando inválido"
        );
        sincronizar_parser(parser);
    }
}

void analisar_comando_iniciado_por_id(Parser *parser){
    consumir_token(parser, TOKEN_ID);

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        analisar_expressao(parser);
        consumir_token(parser, TOKEN_SEMICOLON);

    } else if(parser->current_token.type == TOKEN_LPAREN){
        consumir_token(parser, TOKEN_LPAREN);

        if(parser->current_token.type != TOKEN_RPAREN){
            analisar_lista_de_argumentos(parser);

            if(parser->current_token.type != TOKEN_RPAREN){
                erro_de_sintaxe(
                    parser,
                    "Esperado ')' ao final da chamada de função"
                );
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
        erro_de_sintaxe(
            parser,
            "Esperado atribuição, chamada de função, incremento ou decremento"
        );

        sincronizar_parser(parser);
    }
}

void analisar_declaracao(Parser *parser){
    analisar_tipo(parser);

    consumir_token(parser, TOKEN_ID);

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        analisar_expressao(parser);
    }

    if(parser->current_token.type == TOKEN_SEMICOLON){
        consumir_token(parser, TOKEN_SEMICOLON);
    } else {
        erro_de_sintaxe(parser, "Esperado ';' ao final da declaração");
        sincronizar_ate(parser, TOKEN_SEMICOLON);
    }
}

void analisar_declaracao_sem_ponto_virgula(Parser *parser){
    analisar_tipo(parser);
    consumir_token(parser, TOKEN_ID);

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        analisar_expressao(parser);
    }
}

void analisar_atribuicao(Parser *parser){
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);
    analisar_expressao(parser);
    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_atribuicao_sem_ponto_virgula(Parser *parser){
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);
    analisar_expressao(parser);
}

void analisar_if(Parser *parser){
    consumir_token(parser, TOKEN_IF);

    consumir_token(parser, TOKEN_LPAREN);
    analisar_condicao(parser);
    consumir_token(parser, TOKEN_RPAREN);

    analisar_comando(parser);

    if(parser->current_token.type == TOKEN_ELSE){
        consumir_token(parser, TOKEN_ELSE);
        analisar_comando(parser);
    }
}

void analisar_while(Parser *parser){
    consumir_token(parser, TOKEN_WHILE);

    consumir_token(parser, TOKEN_LPAREN);
    analisar_condicao(parser);
    consumir_token(parser, TOKEN_RPAREN);

    analisar_comando(parser);
}

void analisar_for(Parser *parser){
    consumir_token(parser, TOKEN_FOR);
    consumir_token(parser, TOKEN_LPAREN);

    analisar_inicializacao_for(parser);
    consumir_token(parser, TOKEN_SEMICOLON);

    analisar_condicao(parser);
    consumir_token(parser, TOKEN_SEMICOLON);

    analisar_expressao_de_incremento(parser);

    consumir_token(parser, TOKEN_RPAREN);

    analisar_comando(parser);
}

void analisar_inicializacao_for(Parser *parser){
    if(parser->current_token.type == TOKEN_INT ||
       parser->current_token.type == TOKEN_CHAR ||
       parser->current_token.type == TOKEN_VOID){

        analisar_declaracao_sem_ponto_virgula(parser);

    } else if(parser->current_token.type == TOKEN_ID){

        analisar_atribuicao_sem_ponto_virgula(parser);

    } else {
        erro_de_sintaxe(parser, "inicializacao invalida no for");
    }
}

void analisar_return(Parser *parser){
    consumir_token(parser, TOKEN_RETURN);

    if(parser->current_token.type != TOKEN_SEMICOLON){
        analisar_expressao(parser);
    }

    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_break(Parser *parser){
    consumir_token(parser, TOKEN_BREAK);
    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_continue(Parser *parser){
    consumir_token(parser, TOKEN_CONTINUE);
    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_tipo(Parser *parser){
    if(token_eh_tipo(parser->current_token.type)){
        avancar_token(parser);

    } else {
        erro_de_sintaxe(parser, "Tipo esperado");
    }
}

void analisar_incremento_decremento(Parser *parser){
    if(parser->current_token.type == TOKEN_INCREMENT){
        consumir_token(parser, TOKEN_INCREMENT);
        consumir_token(parser, TOKEN_ID);
        consumir_token(parser, TOKEN_SEMICOLON);
    } else if(parser->current_token.type == TOKEN_DECREMENT){
        consumir_token(parser, TOKEN_DECREMENT);
        consumir_token(parser, TOKEN_ID);
        consumir_token(parser, TOKEN_SEMICOLON);
    } else {
        erro_de_sintaxe(parser, "incremento ou decremento esperado");
    }
}

int token_eh_operador_relacional(TokenType type){
    return type == TOKEN_LT  ||
           type == TOKEN_GT  ||
           type == TOKEN_LTE ||
           type == TOKEN_GTE ||
           type == TOKEN_EQ  ||
           type == TOKEN_NEQ;
}

int token_eh_tipo(TokenType type){
    return type == TOKEN_INT  ||
           type == TOKEN_CHAR ||
           type == TOKEN_VOID;
}

void analisar_condicao(Parser *parser){
    analisar_condicao_relacional(parser);

    while(parser->current_token.type == TOKEN_AND ||
          parser->current_token.type == TOKEN_OR){

        if(parser->current_token.type == TOKEN_AND){
            consumir_token(parser, TOKEN_AND);
        } else {
            consumir_token(parser, TOKEN_OR);
        }

        analisar_condicao_relacional(parser);
    }
}

void analisar_condicao_relacional(Parser *parser){
    analisar_expressao(parser);

    if(parser->current_token.type == TOKEN_LT ||
       parser->current_token.type == TOKEN_GT ||
       parser->current_token.type == TOKEN_LTE ||
       parser->current_token.type == TOKEN_GTE ||
       parser->current_token.type == TOKEN_EQ ||
       parser->current_token.type == TOKEN_NEQ){

        analisar_operador_relacional(parser);
        analisar_expressao(parser);
    }
}

void analisar_operador_relacional(Parser *parser){
    if(token_eh_operador_relacional(parser->current_token.type)){
        avancar_token(parser);

    }else{
        erro_de_sintaxe(
            parser,
            "Operador relacional esperado"
        );
    }
}

void analisar_expressao(Parser *parser){
    analisar_termo(parser);

    while(parser->current_token.type == TOKEN_PLUS ||
          parser->current_token.type == TOKEN_MINUS){

        if(parser->current_token.type == TOKEN_PLUS){
            consumir_token(parser, TOKEN_PLUS);
        } else {
            consumir_token(parser, TOKEN_MINUS);
        }

        analisar_termo(parser);
    }
}

void analisar_expressao_de_incremento(Parser *parser){
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
                "Esperado ++, -- ou = no incremento do for");
        }

    } else if(parser->current_token.type == TOKEN_INCREMENT){

        consumir_token(parser, TOKEN_INCREMENT);
        consumir_token(parser, TOKEN_ID);

    } else if(parser->current_token.type == TOKEN_DECREMENT){

        consumir_token(parser, TOKEN_DECREMENT);
        consumir_token(parser, TOKEN_ID);

    } else {
        erro_de_sintaxe(parser, "Incremento invalido no for");
    }
}

void analisar_termo(Parser *parser){
    analisar_fator(parser);

    while(parser->current_token.type == TOKEN_MULT ||
          parser->current_token.type == TOKEN_DIV){

        if(parser->current_token.type == TOKEN_MULT){
            consumir_token(parser, TOKEN_MULT);
        } else {
            consumir_token(parser, TOKEN_DIV);
        }

        analisar_fator(parser);
    }
}

void analisar_fator(Parser *parser){
    if(parser->current_token.type == TOKEN_MINUS){
        consumir_token(parser, TOKEN_MINUS);
        analisar_fator(parser);

    } else if(parser->current_token.type == TOKEN_NOT){
        consumir_token(parser, TOKEN_NOT);
        analisar_fator(parser);

    } else if(parser->current_token.type == TOKEN_ID){
        consumir_token(parser, TOKEN_ID);

        if(parser->current_token.type == TOKEN_LPAREN){

            consumir_token(parser, TOKEN_LPAREN);

            if(parser->current_token.type != TOKEN_RPAREN){
                analisar_lista_de_argumentos(parser);
            }

            consumir_token(parser, TOKEN_RPAREN);
        }

    } else if(parser->current_token.type == TOKEN_NUM){
        consumir_token(parser, TOKEN_NUM);

    } else if(parser->current_token.type == TOKEN_CHAR_LITERAL){
        consumir_token(parser, TOKEN_CHAR_LITERAL);

    } else if(parser->current_token.type == TOKEN_STRING){
        consumir_token(parser, TOKEN_STRING);

    } else if(parser->current_token.type == TOKEN_LPAREN){
        consumir_token(parser, TOKEN_LPAREN);
        if(parser->current_token.type == TOKEN_RPAREN){
            erro_de_sintaxe(
                parser,
                "Expressão vazia entre parênteses"
            );
        }else{
            analisar_expressao(parser);
        }
        if(parser->current_token.type != TOKEN_RPAREN){

            erro_de_sintaxe(
                parser,
                "Esperado ')' ao final da expressão"
            );

            sincronizar_ate(parser, TOKEN_RPAREN);
        }
        consumir_token(parser, TOKEN_RPAREN);

    }else{
    erro_de_sintaxe(
        parser,
        "Fator inválido na expressão"
    );
    avancar_token(parser);
    }
}

void analisar_lista_de_argumentos(Parser *parser){
    analisar_expressao(parser);

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);

        if(parser->current_token.type == TOKEN_RPAREN){
            erro_de_sintaxe(
                parser,
                "Esperada expressão após vírgula"
            );

            sincronizar_ate(parser, TOKEN_RPAREN);
            return;
        }
        analisar_expressao(parser);
    }
}

void analisar_chamada_funcao(Parser *parser){
    consumir_token(parser, TOKEN_ID);

    consumir_token(parser, TOKEN_LPAREN);

    if(parser->current_token.type != TOKEN_RPAREN){
        analisar_lista_de_argumentos(parser);
    }

    consumir_token(parser, TOKEN_RPAREN);
}