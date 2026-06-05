#ifndef PARSER_H
#define PARSER_H

#include "lexer_v1.h"
#include "ast.h"

#define RED     "\x1b[31m"
#define YELLOW  "\x1b[33m"
#define CYAN    "\x1b[36m"
#define GREEN   "\x1b[32m"
#define RESET   "\x1b[0m"

typedef struct {
    Lexer *lexer;
    Token current_token;
    int quantidade_erros;
    int em_recuperacao;
} Parser;

// =========== Infraestrutura do Parser ===========

void inicializar_parser(Parser *parser, Lexer *lexer);
void avancar_token(Parser *parser);

// Retornam 0 para indicar falha — permite propagação de erro
int  erro_de_sintaxe(Parser *parser, const char *mensagem);
int  consumir_token(Parser *parser, TokenType tipo_esperado);

void sincronizar_parser(Parser *parser);
void sincronizar_ate(Parser *parser, TokenType token);
void mostrar_linha_erro(Parser *parser);
const char *token_para_simbolo(TokenType type);

// =========== Análise Geral do Programa ===========

ASTNode *analisar_programa(Parser *parser);
ASTNode *analisar_lista_de_funcoes(Parser *parser);
ASTNode *analisar_funcao(Parser *parser);
ASTNode *analisar_lista_de_parametros(Parser *parser);
ASTNode *analisar_parametro(Parser *parser);
ASTNode *analisar_bloco(Parser *parser);
ASTNode *analisar_lista_de_comandos(Parser *parser);
ASTNode *analisar_comando(Parser *parser);
ASTNode *analisar_comando_iniciado_por_id(Parser *parser);

// =========== Comandos ===========

ASTNode *analisar_declaracao(Parser *parser);
ASTNode *analisar_declaracao_sem_ponto_virgula(Parser *parser);
ASTNode *analisar_atribuicao(Parser *parser);
ASTNode *analisar_atribuicao_sem_ponto_virgula(Parser *parser);
ASTNode *analisar_if(Parser *parser);
ASTNode *analisar_while(Parser *parser);
ASTNode *analisar_for(Parser *parser);
ASTNode *analisar_inicializacao_for(Parser *parser);
ASTNode *analisar_return(Parser *parser);
ASTNode *analisar_break(Parser *parser);
ASTNode *analisar_continue(Parser *parser);
ASTNode *analisar_incremento_decremento(Parser *parser);
ASTNode *analisar_expressao_de_incremento(Parser *parser);

// =========== Tipos e Auxiliares ===========

int analisar_tipo(Parser *parser);
int token_eh_operador_relacional(TokenType type);
int token_eh_tipo(TokenType type);

// =========== Condições ===========

ASTNode *analisar_condicao(Parser *parser);
ASTNode *analisar_condicao_relacional(Parser *parser);
void analisar_operador_relacional(Parser *parser);

// =========== Expressões ===========

ASTNode *analisar_expressao(Parser *parser);
ASTNode *analisar_termo(Parser *parser);
ASTNode *analisar_fator(Parser *parser);
ASTNode *analisar_lista_de_argumentos(Parser *parser);
ASTNode *analisar_chamada_funcao(Parser *parser, const char *name, int line, int column);

#endif // PARSER_H