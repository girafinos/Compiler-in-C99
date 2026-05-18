#ifndef PARSER_H
#define PARSER_H

#include "lexer_v1.h"

typedef struct {
    Lexer *lexer;
    Token current_token;
} Parser;

// =========== Infraestrutura do Parser ===========

void inicializar_parser(Parser *parser, Lexer *lexer);
void avancar_token(Parser *parser);

void erro_de_sintaxe(Parser *parser, const char *mensagem);
void consumir_token(Parser *parser, TokenType tipo_esperado);

// =========== Análise Geral do Programa ==========


void analisar_programa(Parser *parser);

void analisar_lista_de_funcoes(Parser *parser);
void analisar_funcao(Parser *parser);

void analisar_lista_de_parametros(Parser *parser);
void analisar_parametro(Parser *parser);

void analisar_lista_de_comandos(Parser *parser);
void analisar_comando(Parser *parser);
void analisar_comando_iniciado_por_id(Parser *parser);
void analisar_bloco(Parser *parser);

// =========== Comandos ==========

void analisar_declaracao(Parser *parser);
void analisar_declaracao_sem_ponto_virgula(Parser *parser);
void analisar_atribuicao(Parser *parser);
void analisar_atribuicao_sem_ponto_virgula(Parser *parser);
void analisar_if(Parser *parser);
void analisar_while(Parser *parser);
void analisar_for(Parser *parser); 
void analisar_inicializacao_for(Parser *parser);
void analisar_return(Parser *parser);

// =========== Tipos ==========

void analisar_tipo(Parser *parser);
void analisar_incremento_decremento(Parser *parser);

// =========== Condições ==========

void analisar_condicao(Parser *parser);
void analisar_condicao_relacional(Parser *parser);
void analisar_operador_relacional(Parser *parser);

// =========== Expressões ==========

void analisar_expressao(Parser *parser);
void analisar_termo(Parser *parser);
void analisar_fator(Parser *parser);
void analisar_lista_de_argumentos(Parser *parser);
void analisar_chamada_funcao(Parser *parser);

#endif // PARSER_H