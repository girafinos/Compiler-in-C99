#define _POSIX_C_SOURCE 200809L
#include "ast.h"
#include <stdio.h>
#include <string.h>

ASTNode *ast_new_node(ASTNodeType type, int line, int column){
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

ASTNode *ast_new_identifier(const char *name, int line, int column){
    ASTNode *node = ast_new_node(AST_IDENTIFIER, line, column);
    if(!node) return NULL;
    node->name = strdup(name);
    return node;
}

ASTNode *ast_new_literal(const char *lexeme, TokenType token_type, int line, int column){
    ASTNode *node = ast_new_node(AST_LITERAL, line, column);
    if(!node) return NULL;
    node->lexeme = strdup(lexeme);
    node->value_type = token_type;
    return node;
}

ASTNode *ast_append(ASTNode *list, ASTNode *node){
    if(!node) return list;
    if(!list) return node;
    ASTNode *cursor = list;
    while(cursor->next) cursor = cursor->next;
    cursor->next = node;
    return list;
}

static void ast_print_indent(int indent){
    for(int i = 0; i < indent; i++) printf("  ");
}

static const char *ast_type_name(ASTNodeType type){
    switch(type){
        case AST_PROGRAM: return "Program";
        case AST_FUNCTION_DECL: return "FunctionDecl";
        case AST_PARAMETER: return "Parameter";
        case AST_BLOCK: return "Block";
        case AST_DECLARATION: return "Declaration";
        case AST_ASSIGNMENT: return "Assignment";
        case AST_IF: return "If";
        case AST_WHILE: return "While";
        case AST_FOR: return "For";
        case AST_RETURN: return "Return";
        case AST_BREAK: return "Break";
        case AST_CONTINUE: return "Continue";
        case AST_EXPRESSION_STATEMENT: return "ExprStmt";
        case AST_FUNCTION_CALL: return "Call";
        case AST_BINARY_EXPR: return "BinaryExpr";
        case AST_UNARY_EXPR: return "UnaryExpr";
        case AST_LITERAL: return "Literal";
        case AST_IDENTIFIER: return "Identifier";
        default: return "Unknown";
    }
}

static void ast_print_node(ASTNode *node, int indent){
    if(!node) return;
    ast_print_indent(indent);
    printf("%s", ast_type_name(node->type));
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
            ast_print(node->left, indent + 1);
            break;
        case AST_FUNCTION_DECL:
            ast_print(node->left, indent + 1);
            ast_print(node->right, indent + 1);
            break;
        case AST_BLOCK:
        case AST_EXPRESSION_STATEMENT:
            ast_print(node->left, indent + 1);
            break;
        case AST_DECLARATION:
            ast_print(node->left, indent + 1);
            break;
        case AST_ASSIGNMENT:
            ast_print(node->left, indent + 1);
            break;
        case AST_IF:
            ast_print(node->left, indent + 1);
            ast_print(node->right, indent + 1);
            ast_print(node->third, indent + 1);
            break;
        case AST_WHILE:
            ast_print(node->left, indent + 1);
            ast_print(node->right, indent + 1);
            break;
        case AST_FOR:
            ast_print(node->left, indent + 1);
            ast_print(node->right, indent + 1);
            ast_print(node->third, indent + 1);
            ast_print(node->extra, indent + 1);
            break;
        case AST_RETURN:
            ast_print(node->left, indent + 1);
            break;
        case AST_FUNCTION_CALL:
            ast_print(node->left, indent + 1);
            break;
        case AST_BINARY_EXPR:
        case AST_UNARY_EXPR:
            ast_print(node->left, indent + 1);
            ast_print(node->right, indent + 1);
            break;
        default:
            break;
    }
}

void ast_print(ASTNode *node, int indent){
    for(ASTNode *cursor = node; cursor; cursor = cursor->next){
        ast_print_node(cursor, indent);
    }
}

void ast_free(ASTNode *node){
    while(node){
        ASTNode *next = node->next;
        if(node->name) free(node->name);
        if(node->lexeme) free(node->lexeme);
        ast_free(node->left);
        ast_free(node->right);
        ast_free(node->third);
        ast_free(node->extra);
        free(node);
        node = next;
    }
}
