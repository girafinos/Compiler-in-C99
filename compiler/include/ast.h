#ifndef AST_H
#define AST_H

#include "lexer_v1.h"
#include <stdlib.h>

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION_DECL,
    AST_PARAMETER,
    AST_BLOCK,
    AST_DECLARATION,
    AST_ASSIGNMENT,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_EXPRESSION_STATEMENT,
    AST_FUNCTION_CALL,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_LITERAL,
    AST_IDENTIFIER
} ASTNodeType;

typedef struct ASTNode ASTNode;
struct ASTNode {
    ASTNodeType type;
    ASTNode *next;
    int line;
    int column;
    char *name;
    char *lexeme;
    TokenType value_type;
    ASTNode *left;
    ASTNode *right;
    ASTNode *third;
    ASTNode *extra;
};

ASTNode *ast_novo_no(ASTNodeType type, int line, int column);
ASTNode *ast_novo_identificador(const char *name, int line, int column);
ASTNode *ast_novo_literal(const char *lexeme, TokenType token_type, int line, int column);
ASTNode *ast_anexar(ASTNode *list, ASTNode *node);
void ast_imprimir(ASTNode *node, int indent);
void ast_liberar(ASTNode *node);

#endif // AST_H
