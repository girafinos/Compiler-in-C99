#include "parser.h"

void inicializar_parser(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->current_token = pegar_prox_token(lexer);
}

void avançar_token(Parser *parser) {
    parser->current_token = pegar_prox_token(parser->lexer);
}

void erro_de_sintaxe(Parser *parser, const char *mensagem){
    printf("Erro sintático: %s\n", mensagem);
    printf("Token encontrado: %s | lexema: \"%s\" | line: %d | column: %d\n",
           token_para_string(parser->current_token.type),
           parser->current_token.lexema,
           parser->current_token.line,
           parser->current_token.column);
           exit(1);
}

void consumir_token(Parser *parser, TokenType tipo_esperado){
    if(parser->current_token.type == tipo_esperado){
        avançar_token(parser);
    } else {
        printf("Esperado: %s\n", token_para_string(tipo_esperado));
        erro_de_sintaxe(parser, "Token inesperado");
    }
}

void analisar_programa(Parser *parser){
    analisar_lista_de_comandos(parser);
    consumir_token(parser, TOKEN_EOF);
}

void analisar_lista_de_comandos(Parser *parser){
    while(parser->current_token.type != TOKEN_EOF && parser->current_token.type != TOKEN_RBRACE){
        analisar_comando(parser);
    }
}

void analisar_comando(Parser *parser){
    if(parser->current_token.type == TOKEN_INT){
        analisar_declaracoes(parser);
    } else if(parser->current_token.type == TOKEN_ID){
        analisar_atribuições(parser);
    } else if(parser->current_token.type == TOKEN_LBRACE){
        analisar_bloco(parser);
    } else if(parser->current_token.type == TOKEN_IF){
        analisar_if(parser);
    } else if(parser->current_token.type == TOKEN_WHILE){
        analisar_while(parser);
    } else if(parser->current_token.type == TOKEN_RETURN){
        analisar_return(parser);
    } else {
        erro_de_sintaxe(parser, "Esperado declaração de variável ou atribuição");
    }
}

void analisar_declaracoes(Parser *parser){
    consumir_token(parser, TOKEN_INT);
    consumir_token(parser, TOKEN_ID);
    if(parser->current_token.type == TOKEN_ASSIGN){  
        consumir_token(parser, TOKEN_ASSIGN);
        analisar_expressao(parser);
    }
    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_atribuições(Parser *parser){
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);
    analisar_expressao(parser);
    consumir_token(parser, TOKEN_SEMICOLON);
}

void analisar_expressao(Parser *parser){
    analisar_termo(parser);

    while(parser->current_token.type == TOKEN_PLUS || parser->current_token.type == TOKEN_MINUS){
        if(parser->current_token.type == TOKEN_PLUS){
            consumir_token(parser, TOKEN_PLUS);
        } else {
            consumir_token(parser, TOKEN_MINUS);
        }
        analisar_termo(parser);
    }
}

void analisar_termo(Parser *parser){
    analisar_fator(parser);

    while(parser->current_token.type == TOKEN_MULT || parser->current_token.type == TOKEN_DIV) {
        if(parser->current_token.type == TOKEN_MULT){
            consumir_token(parser, TOKEN_MULT);
        } else {
            consumir_token(parser, TOKEN_DIV);
        }
        analisar_fator(parser);
    }
}

void analisar_fator(Parser *parser){
    if(parser->current_token.type == TOKEN_ID){
        consumir_token(parser, TOKEN_ID);
    } else if(parser->current_token.type == TOKEN_NUM){
        consumir_token(parser, TOKEN_NUM);  
    } else if(parser->current_token.type == TOKEN_LPAREN){
        consumir_token(parser, TOKEN_LPAREN);
        analisar_expressao(parser);
        consumir_token(parser, TOKEN_RPAREN);
    } else {
        erro_de_sintaxe(parser, "Esperado identificador, número ou expressão entre parênteses");
    }
}

void analisar_bloco(Parser *parser){
    consumir_token(parser, TOKEN_LBRACE);
    analisar_lista_de_comandos(parser);
    consumir_token(parser, TOKEN_RBRACE);
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

void analisar_condicao(Parser *parser){
    analisar_condicao_relacional(parser);   

    while(parser->current_token.type == TOKEN_AND || parser->current_token.type == TOKEN_OR){
        if(parser->current_token.type == TOKEN_AND){
            consumir_token(parser, TOKEN_AND);
        } else {
            consumir_token(parser, TOKEN_OR);
        }
        analisar_condicao_relacional(parser);
    }
}

void analisar_operador_relacional(Parser *parser){
    if (parser->current_token.type == TOKEN_LT) {
        consumir_token(parser, TOKEN_LT);
    } else if (parser->current_token.type == TOKEN_GT) {
        consumir_token(parser, TOKEN_GT);
    } else if (parser->current_token.type == TOKEN_LTE) {
        consumir_token(parser, TOKEN_LTE);
    } else if (parser->current_token.type == TOKEN_GTE) {
        consumir_token(parser, TOKEN_GTE);
    } else if (parser->current_token.type == TOKEN_EQ) {
        consumir_token(parser, TOKEN_EQ);
    } else if (parser->current_token.type == TOKEN_NEQ) {
        consumir_token(parser, TOKEN_NEQ);
    } else {
        erro_de_sintaxe(parser, "operador relacional esperado");
    }
}

void analisar_condicao_relacional(Parser *parser) {
    analisar_expressao(parser);

    if (parser->current_token.type == TOKEN_LT ||
        parser->current_token.type == TOKEN_GT ||
        parser->current_token.type == TOKEN_LTE ||
        parser->current_token.type == TOKEN_GTE ||
        parser->current_token.type == TOKEN_EQ ||
        parser->current_token.type == TOKEN_NEQ) {

        analisar_operador_relacional(parser);
        analisar_expressao(parser);
    }
}

void analisar_return(Parser *parser){
    consumir_token(parser, TOKEN_RETURN);

    if (parser->current_token.type != TOKEN_SEMICOLON)
    {
        analisar_expressao(parser);
    }
    
    consumir_token(parser, TOKEN_SEMICOLON);
}