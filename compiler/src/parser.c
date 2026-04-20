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

void come(Parser *parser, TokenType tipo_esperado){
    if(parser->current_token.type == tipo_esperado){
        avançar_token(parser);
    } else {
        printf("Esperado: %s\n", token_para_string(tipo_esperado));
        erro_de_sintaxe(parser, "Token inesperado");
    }
}