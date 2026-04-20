#ifndef PARSER_H
#define PARSER_H

#include "lexer_v1.h"

typedef struct {
    Lexer *lexer;
    Token current_token;
} Parser;   

void inicializar_parser(Parser *parser, Lexer *lexer);
void avançar_token(Parser *parser);
void erro_de_sintaxe(Parser *parser, const char *mensagem);
void come(Parser *parser, TokenType tipo_esperado);

#endif // PARSER_H