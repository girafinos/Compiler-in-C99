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
void consumir_token(Parser *parser, TokenType tipo_esperado);

void analisar_programa(Parser *parser);
void analisar_lista_de_comandos(Parser *parser);
void analisar_comando(Parser *parser);
void analisar_declaracoes(Parser *parser);
void analisar_atribuições(Parser *parser);
void analisar_expressao(Parser *parser);
void analisar_termo(Parser *parser);
void analisar_fator(Parser *parser);

#endif // PARSER_H