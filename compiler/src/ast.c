#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <string.h>

ASTNode *ast_novo_no(ASTNodeType type, int line, int column){
    ASTNode *node = malloc(sizeof(ASTNode));
    if(!node) return NULL;
    node->type = type;
    node->next = NULL;
    node->line = line;
    node->column = column;
    node->name = NULL;
    node->lexeme = NULL;
    node->value_type = TOKEN_ERROR;
    node->left = NULL;
    node->right = NULL;
    node->third = NULL;
    node->extra = NULL;
    return node;
}

ASTNode *ast_novo_identificador(const char *name, int line, int column){
    ASTNode *node = ast_novo_no(AST_IDENTIFIER, line, column);
    if(!node) return NULL;
    node->name = strdup(name);
    return node;
}

ASTNode *ast_novo_literal(const char *lexeme, TokenType token_type, int line, int column){
    ASTNode *node = ast_novo_no(AST_LITERAL, line, column);
    if(!node) return NULL;
    node->lexeme = strdup(lexeme);
    node->value_type = token_type;
    return node;
}

ASTNode *ast_anexar(ASTNode *list, ASTNode *node){
    if(!node) return list;
    if(!list) return node;
    ASTNode *cursor = list;
    while(cursor->next) cursor = cursor->next;
    cursor->next = node;
    return list;
}

static void ast_imprimir_indentacao(int indent){
    for(int i = 0; i < indent; i++) printf("  ");
}

static const char *ast_nome_tipo(ASTNodeType type){
    switch(type){
        case AST_PROGRAM: return "Programa";
        case AST_FUNCTION_DECL: return "Funcao";
        case AST_PARAMETER: return "Parametro";
        case AST_BLOCK: return "Bloco";
        case AST_DECLARATION: return "Declaracao";
        case AST_ASSIGNMENT: return "Atribuicao";
        case AST_IF: return "Se";
        case AST_WHILE: return "Enquanto";
        case AST_FOR: return "Para";
        case AST_RETURN: return "Retorno";
        case AST_BREAK: return "Break";
        case AST_CONTINUE: return "Continue";
        case AST_EXPRESSION_STATEMENT: return "Expressao";
        case AST_FUNCTION_CALL: return "Chamada";
        case AST_BINARY_EXPR: return "ExprBinaria";
        case AST_UNARY_EXPR: return "ExprUnaria";
        case AST_LITERAL: return "Literal";
        case AST_IDENTIFIER: return "Identificador";
        default: return "Desconhecido";
    }
}

static void ast_imprimir_no(ASTNode *node, int indent){
    if(!node) return;
    ast_imprimir_indentacao(indent);
    printf("%s", ast_nome_tipo(node->type));
    switch(node->type){
        case AST_FUNCTION_DECL:
            printf(" name=%s", node->name ? node->name : "<anon>");
            break;
        case AST_PARAMETER:
            printf(" %s", node->name ? node->name : "<param>");
            break;
        case AST_DECLARATION:
            printf(" name=%s", node->name ? node->name : "<decl>");
            break;
        case AST_ASSIGNMENT:
            printf(" name=%s", node->name ? node->name : "<assign>");
            break;
        case AST_FUNCTION_CALL:
            printf(" name=%s", node->name ? node->name : "<call>");
            break;
        case AST_BINARY_EXPR:
            printf(" op=%s", token_para_string(node->value_type));
            break;
        case AST_UNARY_EXPR:
            printf(" op=%s", token_para_string(node->value_type));
            break;
        case AST_LITERAL:
            printf(" lexeme=%s", node->lexeme ? node->lexeme : "<lit>");
            break;
        case AST_IDENTIFIER:
            printf(" name=%s", node->name ? node->name : "<id>");
            break;
        default:
            break;
    }
    printf("\n");

    switch(node->type){
        case AST_PROGRAM:
            ast_imprimir(node->left, indent + 1);
            break;
        case AST_FUNCTION_DECL:
            ast_imprimir(node->left, indent + 1);
            ast_imprimir(node->right, indent + 1);
            break;
        case AST_BLOCK:
        case AST_EXPRESSION_STATEMENT:
            ast_imprimir(node->left, indent + 1);
            break;
        case AST_DECLARATION:
            ast_imprimir(node->left, indent + 1);
            break;
        case AST_ASSIGNMENT:
            ast_imprimir(node->left, indent + 1);
            break;
        case AST_IF:
            ast_imprimir(node->left, indent + 1);
            ast_imprimir(node->right, indent + 1);
            ast_imprimir(node->third, indent + 1);
            break;
        case AST_WHILE:
            ast_imprimir(node->left, indent + 1);
            ast_imprimir(node->right, indent + 1);
            break;
        case AST_FOR:
            ast_imprimir(node->left, indent + 1);
            ast_imprimir(node->right, indent + 1);
            ast_imprimir(node->third, indent + 1);
            ast_imprimir(node->extra, indent + 1);
            break;
        case AST_RETURN:
            ast_imprimir(node->left, indent + 1);
            break;
        case AST_FUNCTION_CALL:
            ast_imprimir(node->left, indent + 1);
            break;
        case AST_BINARY_EXPR:
        case AST_UNARY_EXPR:
            ast_imprimir(node->left, indent + 1);
            ast_imprimir(node->right, indent + 1);
            break;
        default:
            break;
    }
}

void ast_imprimir(ASTNode *node, int indent){
    for(ASTNode *cursor = node; cursor; cursor = cursor->next){
        ast_imprimir_no(cursor, indent);
    }
}

void ast_liberar(ASTNode *node){
    while(node){
        ASTNode *next = node->next;
        if(node->name) free(node->name);
        if(node->lexeme) free(node->lexeme);
        ast_liberar(node->left);
        ast_liberar(node->right);
        ast_liberar(node->third);
        ast_liberar(node->extra);
        free(node);
        node = next;
    }
}
