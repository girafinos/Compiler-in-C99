#ifndef LEXER_H
#define LEXER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    //Identificadores e literais
    TOKEN_ID,
    TOKEN_NUM,  
    TOKEN_STRING,
    TOKEN_CHAR_LITERAL,

    //Palavras reservadas
    TOKEN_INT,
    TOKEN_CHAR,
    TOKEN_CONST,
    TOKEN_VOID,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_RETURN,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_TYPEDEF,
    TOKEN_STRUCT,
    TOKEN_ENUM,
    TOKEN_BREAK,

    //Operadores
    TOKEN_PLUS,        //+
    TOKEN_MINUS,       //-
    TOKEN_ASSIGN,      //=
    TOKEN_MULT,        //*
    TOKEN_DIV,         // /
    TOKEN_LT,          //<
    TOKEN_GT,          //>
    TOKEN_NOT,         //!

    //Caracteres especiais
    TOKEN_HASH,        //#
    TOKEN_DOT,         //.
    TOKEN_AMPERSAND,   //&
    TOKEN_PIPE,        //|

    //Operadores compostos
    TOKEN_EQ,          // ==
    TOKEN_NEQ,         // !=
    TOKEN_LTE,         // <=
    TOKEN_GTE,         // >=
    TOKEN_AND,         // &&
    TOKEN_OR,          // ||

    //Delimitadores
    TOKEN_SEMICOLON,   //;
    TOKEN_COMMA,       //,

    TOKEN_LPAREN,      //(
    TOKEN_RPAREN,      //)
    TOKEN_LBRACE,      //{
    TOKEN_RBRACE,      //}
    TOKEN_LBRACKET,    //[
    TOKEN_RBRACKET,    //],

    //Fim de arquivo e erro
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexema[100];
    int line;
    int column;
} Token;

typedef struct {
    const char *src;
    int pos;
    int line;
    int column;
    char current_char;  
} Lexer;

void inicializar_lexer(Lexer *lexer, const char *source); //Inicializa o lexer com a fonte de entrada
void andar_char(Lexer *lexer); //Avança para o próximo caractere
char spoiler_prox_char(Lexer *lexer); //Olha o próximo caractere sem avançar
void pular_espacos(Lexer *lexer); //Ignora espaços em branco
Token cria_token(TokenType type, const char* lexema, int line, int column); //Cria um token
TokenType palavra_chave_ou_id(const char *lexema); //Determina se um lexema é uma palavra reservada ou um identificador
Token identificadores(Lexer *lexer); //Processa identificadores e palavras reservadas
Token numeros(Lexer *lexer); //Processa números
Token string_literal(Lexer *lexer); //Processa literais de string
Token pegar_prox_token(Lexer *lexer); //Obtém o próximo token da fonte de entrada
Token char_literal(Lexer *lexer); //Processa literais de caractere
void print_token(Token token); //Imprime um token para depuração
const char* token_para_string(TokenType type); //Converte um TokenType para string (depuração)
char *ler_arquivo(const char *filename); //Lê o conteúdo de um arquivo e retorna como string
void pular_comentario_linha(Lexer *lexer); //Ignora linhas de comentário
void pular_comentario_bloco(Lexer *lexer); //Ignora blocos
#endif 