#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
//  INFRAESTRUTURA DO PARSER
// =============================================================================

void inicializar_parser(Parser *parser, Lexer *lexer){
    parser->lexer          = lexer;
    parser->current_token  = pegar_prox_token(lexer);
    parser->em_recuperacao = 0;
    parser->quantidade_erros = 0;
}

void avancar_token(Parser *parser){
    parser->current_token = pegar_prox_token(parser->lexer);
}

int erro_de_sintaxe(Parser *parser, const char *mensagem){
    if(parser->em_recuperacao) return 0;

    parser->em_recuperacao = 1;
    parser->quantidade_erros++;
    printf("\n");

    printf(RED
           "[ ERRO SINTÁTICO #%d ]\n"
           RESET,
           parser->quantidade_erros);

    printf(YELLOW "Mensagem:\n" RESET);
    printf("  %s\n\n", mensagem);

    printf(YELLOW "Localização:\n" RESET);
    printf("  Linha: %d\n", parser->current_token.line);
    printf("  Coluna: %d\n\n", parser->current_token.column);

    printf(YELLOW "Token encontrado:\n" RESET);
    printf("  Tipo: %s\n", token_para_string(parser->current_token.type));
    printf("  Símbolo: %s\n", token_para_simbolo(parser->current_token.type));
    printf("  Lexema: \"%s\"\n\n", parser->current_token.lexema);

    printf(YELLOW "Trecho:\n" RESET);
    mostrar_linha_erro(parser);

    return 0;
}

int consumir_token(Parser *parser, TokenType tipo_esperado){
    if(parser->em_recuperacao) return 0;

    if(parser->current_token.type == tipo_esperado){
        avancar_token(parser);
        return 1;
    }

    char mensagem[200];
    sprintf(mensagem,
            "Esperado %s, mas encontrado %s",
            token_para_simbolo(tipo_esperado),
            token_para_simbolo(parser->current_token.type));

    return erro_de_sintaxe(parser, mensagem);
}

void sincronizar_parser(Parser *parser){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == TOKEN_SEMICOLON){
            avancar_token(parser);
            break;
        }
        if(parser->current_token.type == TOKEN_RBRACE){
            break;
        }
        avancar_token(parser);
    }
    parser->em_recuperacao = 0;
}

void sincronizar_ate(Parser *parser, TokenType token){
    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == token){
            parser->em_recuperacao = 0;
            return;
        }
        avancar_token(parser);
    }
    parser->em_recuperacao = 0;
}

void mostrar_linha_erro(Parser *parser){
    const char *source = parser->lexer->src;
    int linha_atual = 1;
    int i = 0;

    while(source[i] != '\0'){
        if(linha_atual == parser->current_token.line){
            printf(CYAN "%4d | " RESET, linha_atual);

            while(source[i] != '\n' && source[i] != '\0'){
                printf("%c", source[i]);
                i++;
            }

            printf("\n       ");
            for(int j = 1; j < parser->current_token.column; j++){
                printf(" ");
            }
            printf(RED "^\n" RESET);
            return;
        }
        if(source[i] == '\n') linha_atual++;
        i++;
    }
}

const char *token_para_simbolo(TokenType type){
    switch(type){
        case TOKEN_INT:          return "int";
        case TOKEN_CHAR:         return "char";
        case TOKEN_VOID:         return "void";
        case TOKEN_IF:           return "if";
        case TOKEN_ELSE:         return "else";
        case TOKEN_WHILE:        return "while";
        case TOKEN_FOR:          return "for";
        case TOKEN_RETURN:       return "return";
        case TOKEN_BREAK:        return "break";
        case TOKEN_CONTINUE:     return "continue";
        case TOKEN_PLUS:         return "+";
        case TOKEN_MINUS:        return "-";
        case TOKEN_MULT:         return "*";
        case TOKEN_DIV:          return "/";
        case TOKEN_ASSIGN:       return "=";
        case TOKEN_EQ:           return "==";
        case TOKEN_NEQ:          return "!=";
        case TOKEN_LT:           return "<";
        case TOKEN_GT:           return ">";
        case TOKEN_LTE:          return "<=";
        case TOKEN_GTE:          return ">=";
        case TOKEN_AND:          return "&&";
        case TOKEN_OR:           return "||";
        case TOKEN_NOT:          return "!";
        case TOKEN_INCREMENT:    return "++";
        case TOKEN_DECREMENT:    return "--";
        case TOKEN_LPAREN:       return "(";
        case TOKEN_RPAREN:       return ")";
        case TOKEN_LBRACE:       return "{";
        case TOKEN_RBRACE:       return "}";
        case TOKEN_COMMA:        return ",";
        case TOKEN_SEMICOLON:    return ";";
        case TOKEN_ID:           return "identificador";
        case TOKEN_NUM:          return "numero";
        case TOKEN_STRING:       return "string";
        case TOKEN_CHAR_LITERAL: return "char literal";
        case TOKEN_EOF:          return "EOF";
        default:                 return "desconhecido";
    }
}

int token_eh_tipo(TokenType type){
    return type == TOKEN_INT  ||
           type == TOKEN_CHAR ||
           type == TOKEN_VOID;
}

int token_eh_operador_relacional(TokenType type){
    return type == TOKEN_LT  ||
           type == TOKEN_GT  ||
           type == TOKEN_LTE ||
           type == TOKEN_GTE ||
           type == TOKEN_EQ  ||
           type == TOKEN_NEQ;
}

int analisar_tipo(Parser *parser){
    if(parser->em_recuperacao) return 0;

    if(token_eh_tipo(parser->current_token.type)){
        avancar_token(parser);
        return 1;
    }

    return erro_de_sintaxe(parser, "Tipo esperado: int, char ou void");
}

ASTNode *analisar_lista_de_argumentos(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *arguments = NULL;
    ASTNode *expr = analisar_expressao(parser);
    if(!expr) return NULL;
    arguments = ast_append(arguments, expr);

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);

        if(parser->current_token.type == TOKEN_RPAREN){
            erro_de_sintaxe(parser, "Falta uma expressão após a vírgula");
            return NULL;
        }

        expr = analisar_expressao(parser);
        if(!expr) return NULL;
        arguments = ast_append(arguments, expr);
    }

    return arguments;
}

ASTNode *analisar_chamada_funcao(Parser *parser, const char *name, int line, int column){
    if(parser->em_recuperacao) return NULL;

    ASTNode *call = ast_new_node(AST_FUNCTION_CALL, line, column);
    if(!call) return NULL;
    call->name = strdup(name);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        ast_free(call);
        return NULL;
    }

    if(parser->current_token.type != TOKEN_RPAREN){
        call->left = analisar_lista_de_argumentos(parser);
        if(parser->em_recuperacao){
            sincronizar_ate(parser, TOKEN_RPAREN);
        }
    }

    consumir_token(parser, TOKEN_RPAREN);
    return call;
}

ASTNode *analisar_fator(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type == TOKEN_MINUS ||
       parser->current_token.type == TOKEN_NOT){
        TokenType op = parser->current_token.type;
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, op);
        ASTNode *operand = analisar_fator(parser);
        if(!operand) return NULL;

        ASTNode *node = ast_new_node(AST_UNARY_EXPR, line, column);
        node->value_type = op;
        node->left = operand;
        return node;
    }

    if(parser->current_token.type == TOKEN_ID){
        char name[100];
        strcpy(name, parser->current_token.lexema);
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, TOKEN_ID);

        if(parser->current_token.type == TOKEN_LPAREN){
            return analisar_chamada_funcao(parser, name, line, column);
        }

        return ast_new_identifier(name, line, column);
    }

    if(parser->current_token.type == TOKEN_NUM ||
       parser->current_token.type == TOKEN_CHAR_LITERAL ||
       parser->current_token.type == TOKEN_STRING){
        ASTNode *literal = ast_new_literal(parser->current_token.lexema,
                                          parser->current_token.type,
                                          parser->current_token.line,
                                          parser->current_token.column);
        consumir_token(parser, parser->current_token.type);
        return literal;
    }

    if(parser->current_token.type == TOKEN_LPAREN){
        consumir_token(parser, TOKEN_LPAREN);

        if(parser->current_token.type == TOKEN_RPAREN){
            erro_de_sintaxe(parser, "Expressão vazia entre parênteses");
            return NULL;
        }

        ASTNode *expression = analisar_expressao(parser);
        if(parser->em_recuperacao){
            sincronizar_ate(parser, TOKEN_RPAREN);
        }

        consumir_token(parser, TOKEN_RPAREN);
        return expression;
    }

    erro_de_sintaxe(parser, "Expressão inválida: fator ausente ou operador isolado");
    return NULL;
}

ASTNode *analisar_termo(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_fator(parser);
    if(!left) return NULL;

    while(parser->current_token.type == TOKEN_MULT ||
          parser->current_token.type == TOKEN_DIV){
        TokenType op = parser->current_token.type;
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_fator(parser);
        if(!right) return NULL;

        ASTNode *binary = ast_new_node(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

ASTNode *analisar_expressao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_termo(parser);
    if(!left) return NULL;

    while(parser->current_token.type == TOKEN_PLUS ||
          parser->current_token.type == TOKEN_MINUS){
        TokenType op = parser->current_token.type;
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_termo(parser);
        if(!right) return NULL;

        ASTNode *binary = ast_new_node(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

ASTNode *analisar_condicao_relacional(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_expressao(parser);
    if(!left) return NULL;

    if(token_eh_operador_relacional(parser->current_token.type)){
        TokenType op = parser->current_token.type;
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_expressao(parser);
        if(!right) return NULL;

        ASTNode *binary = ast_new_node(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left = left;
        binary->right = right;
        return binary;
    }

    return left;
}

ASTNode *analisar_condicao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *left = analisar_condicao_relacional(parser);
    if(!left) return NULL;

    while(parser->current_token.type == TOKEN_AND ||
          parser->current_token.type == TOKEN_OR){
        TokenType op = parser->current_token.type;
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, op);

        ASTNode *right = analisar_condicao_relacional(parser);
        if(!right) return NULL;

        ASTNode *binary = ast_new_node(AST_BINARY_EXPR, line, column);
        binary->value_type = op;
        binary->left = left;
        binary->right = right;
        left = binary;
    }

    return left;
}

void analisar_operador_relacional(Parser *parser){
    if(token_eh_operador_relacional(parser->current_token.type)){
        avancar_token(parser);
    } else {
        erro_de_sintaxe(parser, "Operador relacional ausente na condição");
    }
}

ASTNode *analisar_declaracao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return NULL;
    }

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na declaração");
        sincronizar_parser(parser);
        return NULL;
    }

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    char *name = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    ASTNode *decl = ast_new_node(AST_DECLARATION, line, column);
    decl->name = name;
    decl->value_type = declared_type;

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        decl->left = analisar_expressao(parser);
        if(!decl->left){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return decl;
        }
    }

    if(!consumir_token(parser, TOKEN_SEMICOLON)){
        parser->em_recuperacao = 0;
        return decl;
    }

    return decl;
}

ASTNode *analisar_declaracao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na declaração");
        return NULL;
    }

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    char *name = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    ASTNode *decl = ast_new_node(AST_DECLARATION, line, column);
    decl->name = name;
    decl->value_type = declared_type;

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        decl->left = analisar_expressao(parser);
    }

    return decl;
}

ASTNode *analisar_atribuicao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na atribuição");
        sincronizar_parser(parser);
        return NULL;
    }

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    char *name = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);

    ASTNode *value = analisar_expressao(parser);
    ASTNode *assign = ast_new_node(AST_ASSIGNMENT, line, column);
    assign->name = name;
    assign->left = value;

    if(!value){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
        consumir_token(parser, TOKEN_SEMICOLON);
        return assign;
    }

    consumir_token(parser, TOKEN_SEMICOLON);
    return assign;
}

ASTNode *analisar_atribuicao_sem_ponto_virgula(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado na atribuição");
        return NULL;
    }

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    char *name = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);
    consumir_token(parser, TOKEN_ASSIGN);

    ASTNode *value = analisar_expressao(parser);
    ASTNode *assign = ast_new_node(AST_ASSIGNMENT, line, column);
    assign->name = name;
    assign->left = value;
    return assign;
}

ASTNode *analisar_incremento_decremento(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType op = parser->current_token.type;
    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, op);

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Esperado identificador após ++ ou --");
        return NULL;
    }

    ASTNode *operand = ast_new_identifier(parser->current_token.lexema,
                                         parser->current_token.line,
                                         parser->current_token.column);
    consumir_token(parser, TOKEN_ID);

    ASTNode *node = ast_new_node(AST_UNARY_EXPR, line, column);
    node->value_type = op;
    node->left = operand;
    return node;
}

ASTNode *analisar_comando_iniciado_por_id(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no comando");
        sincronizar_parser(parser);
        return NULL;
    }

    char *name = strdup(parser->current_token.lexema);
    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_ID);

    if(parser->current_token.type == TOKEN_ASSIGN){
        consumir_token(parser, TOKEN_ASSIGN);
        ASTNode *value = analisar_expressao(parser);
        ASTNode *assign = ast_new_node(AST_ASSIGNMENT, line, column);
        assign->name = name;
        assign->left = value;
        if(!value){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return assign;
        }
        consumir_token(parser, TOKEN_SEMICOLON);
        return assign;

    } else if(parser->current_token.type == TOKEN_LPAREN){
        ASTNode *call = analisar_chamada_funcao(parser, name, line, column);
        if(!call){
            free(name);
            sincronizar_ate(parser, TOKEN_SEMICOLON);
            consumir_token(parser, TOKEN_SEMICOLON);
            return NULL;
        }
        consumir_token(parser, TOKEN_SEMICOLON);
        return call;

    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        TokenType op = parser->current_token.type;
        int op_line = parser->current_token.line;
        int op_column = parser->current_token.column;
        consumir_token(parser, op);
        ASTNode *operand = ast_new_identifier(name, line, column);
        ASTNode *node = ast_new_node(AST_UNARY_EXPR, op_line, op_column);
        node->value_type = op;
        node->left = operand;
        consumir_token(parser, TOKEN_SEMICOLON);
        return node;

    } else {
        erro_de_sintaxe(parser,
            "Esperado atribuição, chamada de função, ++ ou -- após o identificador");
        sincronizar_parser(parser);
        free(name);
        return NULL;
    }
}

ASTNode *analisar_comando(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(token_eh_tipo(parser->current_token.type)){
        return analisar_declaracao(parser);

    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        ASTNode *node = analisar_incremento_decremento(parser);
        consumir_token(parser, TOKEN_SEMICOLON);
        return node;

    } else if(parser->current_token.type == TOKEN_ID){
        return analisar_comando_iniciado_por_id(parser);

    } else if(parser->current_token.type == TOKEN_LBRACE){
        return analisar_bloco(parser);

    } else if(parser->current_token.type == TOKEN_IF){
        return analisar_if(parser);

    } else if(parser->current_token.type == TOKEN_WHILE){
        return analisar_while(parser);

    } else if(parser->current_token.type == TOKEN_FOR){
        return analisar_for(parser);

    } else if(parser->current_token.type == TOKEN_RETURN){
        return analisar_return(parser);

    } else if(parser->current_token.type == TOKEN_BREAK){
        return analisar_break(parser);

    } else if(parser->current_token.type == TOKEN_CONTINUE){
        return analisar_continue(parser);

    } else {
        erro_de_sintaxe(parser, "Comando inválido ou não suportado");
        sincronizar_parser(parser);
        return NULL;
    }
}

ASTNode *analisar_lista_de_comandos(Parser *parser){
    ASTNode *list = NULL;

    while(parser->current_token.type != TOKEN_RBRACE &&
          parser->current_token.type != TOKEN_EOF){

        ASTNode *command = analisar_comando(parser);
        if(command) list = ast_append(list, command);

        if(parser->em_recuperacao){
            sincronizar_parser(parser);
        }
    }

    return list;
}

ASTNode *analisar_bloco(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    if(!consumir_token(parser, TOKEN_LBRACE)){
        sincronizar_parser(parser);
        return NULL;
    }

    ASTNode *block = ast_new_node(AST_BLOCK, line, column);
    block->left = analisar_lista_de_comandos(parser);
    consumir_token(parser, TOKEN_RBRACE);
    return block;
}

ASTNode *analisar_parametro(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType declared_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_ate(parser, TOKEN_RPAREN);
        return NULL;
    }

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no parâmetro");
        sincronizar_ate(parser, TOKEN_RPAREN);
        return NULL;
    }

    ASTNode *param = ast_new_node(AST_PARAMETER,
                                 parser->current_token.line,
                                 parser->current_token.column);
    param->name = strdup(parser->current_token.lexema);
    param->value_type = declared_type;
    consumir_token(parser, TOKEN_ID);
    return param;
}

ASTNode *analisar_lista_de_parametros(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    ASTNode *list = analisar_parametro(parser);
    if(!list) return NULL;

    while(parser->current_token.type == TOKEN_COMMA){
        consumir_token(parser, TOKEN_COMMA);
        ASTNode *param = analisar_parametro(parser);
        if(param) list = ast_append(list, param);
    }

    return list;
}

ASTNode *analisar_if(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_IF);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return NULL;
    }

    ASTNode *condition = analisar_condicao(parser);
    if(parser->em_recuperacao){
        sincronizar_ate(parser, TOKEN_RPAREN);
    }
    consumir_token(parser, TOKEN_RPAREN);

    ASTNode *then_branch = analisar_comando(parser);
    ASTNode *else_branch = NULL;
    if(parser->current_token.type == TOKEN_ELSE){
        consumir_token(parser, TOKEN_ELSE);
        else_branch = analisar_comando(parser);
    }

    ASTNode *node = ast_new_node(AST_IF, line, column);
    node->left = condition;
    node->right = then_branch;
    node->third = else_branch;
    return node;
}

ASTNode *analisar_while(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_WHILE);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return NULL;
    }

    ASTNode *condition = analisar_condicao(parser);
    if(parser->em_recuperacao){
        sincronizar_ate(parser, TOKEN_RPAREN);
    }
    consumir_token(parser, TOKEN_RPAREN);

    ASTNode *body = analisar_comando(parser);
    ASTNode *node = ast_new_node(AST_WHILE, line, column);
    node->left = condition;
    node->right = body;
    return node;
}

ASTNode *analisar_inicializacao_for(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(token_eh_tipo(parser->current_token.type)){
        return analisar_declaracao_sem_ponto_virgula(parser);

    } else if(parser->current_token.type == TOKEN_ID){
        return analisar_atribuicao_sem_ponto_virgula(parser);

    } else {
        erro_de_sintaxe(parser, "Inicialização inválida no for");
        return NULL;
    }
}

ASTNode *analisar_expressao_de_incremento(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    if(parser->current_token.type == TOKEN_ID){
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        char *name = strdup(parser->current_token.lexema);
        consumir_token(parser, TOKEN_ID);

        if(parser->current_token.type == TOKEN_INCREMENT ||
           parser->current_token.type == TOKEN_DECREMENT){
            TokenType op = parser->current_token.type;
            consumir_token(parser, op);
            ASTNode *operand = ast_new_identifier(name, line, column);
            free(name);
            ASTNode *node = ast_new_node(AST_UNARY_EXPR, line, column);
            node->value_type = op;
            node->left = operand;
            return node;

        } else if(parser->current_token.type == TOKEN_ASSIGN){
            consumir_token(parser, TOKEN_ASSIGN);
            ASTNode *expr = analisar_expressao(parser);
            ASTNode *assign = ast_new_node(AST_ASSIGNMENT, line, column);
            assign->name = name;
            assign->left = expr;
            return assign;
        }

        free(name);
        erro_de_sintaxe(parser, "Esperado ++, -- ou = na parte de incremento do for");
        return NULL;

    } else if(parser->current_token.type == TOKEN_INCREMENT ||
              parser->current_token.type == TOKEN_DECREMENT){
        TokenType op = parser->current_token.type;
        int line = parser->current_token.line;
        int column = parser->current_token.column;
        consumir_token(parser, op);

        if(parser->current_token.type != TOKEN_ID){
            erro_de_sintaxe(parser, "Esperado identificador após ++/--");
            return NULL;
        }

        ASTNode *operand = ast_new_identifier(parser->current_token.lexema,
                                             parser->current_token.line,
                                             parser->current_token.column);
        consumir_token(parser, TOKEN_ID);

        ASTNode *node = ast_new_node(AST_UNARY_EXPR, line, column);
        node->value_type = op;
        node->left = operand;
        return node;

    } else {
        return NULL;
    }
}

ASTNode *analisar_for(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_FOR);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        return NULL;
    }

    ASTNode *init = analisar_inicializacao_for(parser);
    if(parser->em_recuperacao){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
    }
    consumir_token(parser, TOKEN_SEMICOLON);

    ASTNode *condition = analisar_condicao(parser);
    if(parser->em_recuperacao){
        sincronizar_ate(parser, TOKEN_SEMICOLON);
    }
    consumir_token(parser, TOKEN_SEMICOLON);

    ASTNode *increment = analisar_expressao_de_incremento(parser);
    if(parser->em_recuperacao){
        sincronizar_ate(parser, TOKEN_RPAREN);
    }

    if(!consumir_token(parser, TOKEN_RPAREN)){
        if(parser->current_token.type == TOKEN_LBRACE){
            parser->em_recuperacao = 0;
        } else {
            sincronizar_ate(parser, TOKEN_RPAREN);
            if(parser->current_token.type == TOKEN_RPAREN){
                consumir_token(parser, TOKEN_RPAREN);
            } else {
                sincronizar_parser(parser);
                return NULL;
            }
        }
    }

    ASTNode *body = analisar_comando(parser);
    ASTNode *node = ast_new_node(AST_FOR, line, column);
    node->left = init;
    node->right = condition;
    node->third = increment;
    node->extra = body;
    return node;
}

ASTNode *analisar_return(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_RETURN);

    ASTNode *node = ast_new_node(AST_RETURN, line, column);
    if(parser->current_token.type != TOKEN_SEMICOLON){
        node->left = analisar_expressao(parser);
        if(!node->left){
            sincronizar_ate(parser, TOKEN_SEMICOLON);
        }
    }

    consumir_token(parser, TOKEN_SEMICOLON);
    return node;
}

ASTNode *analisar_break(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_BREAK);
    consumir_token(parser, TOKEN_SEMICOLON);
    return ast_new_node(AST_BREAK, line, column);
}

ASTNode *analisar_continue(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    consumir_token(parser, TOKEN_CONTINUE);
    consumir_token(parser, TOKEN_SEMICOLON);
    return ast_new_node(AST_CONTINUE, line, column);
}

ASTNode *analisar_lista_de_funcoes(Parser *parser){
    ASTNode *functions = NULL;

    while(parser->current_token.type != TOKEN_EOF){
        if(parser->current_token.type == TOKEN_RBRACE){
            erro_de_sintaxe(parser,
                "Chave de fechamento inesperada no escopo global");
            avancar_token(parser);
            parser->em_recuperacao = 0;
            continue;
        }

        ASTNode *func = analisar_funcao(parser);
        if(func) functions = ast_append(functions, func);

        if(parser->em_recuperacao){
            sincronizar_parser(parser);
            if(parser->current_token.type == TOKEN_RBRACE){
                avancar_token(parser);
                parser->em_recuperacao = 0;
            }
        }
    }

    return functions;
}

ASTNode *analisar_funcao(Parser *parser){
    if(parser->em_recuperacao) return NULL;

    TokenType return_type = parser->current_token.type;
    if(!analisar_tipo(parser)){
        sincronizar_parser(parser);
        return NULL;
    }

    if(parser->current_token.type != TOKEN_ID){
        erro_de_sintaxe(parser, "Identificador esperado no nome da função");
        sincronizar_parser(parser);
        return NULL;
    }

    int line = parser->current_token.line;
    int column = parser->current_token.column;
    char *name = strdup(parser->current_token.lexema);
    consumir_token(parser, TOKEN_ID);

    if(!consumir_token(parser, TOKEN_LPAREN)){
        sincronizar_parser(parser);
        free(name);
        return NULL;
    }

    ASTNode *params = NULL;
    if(parser->current_token.type != TOKEN_RPAREN){
        params = analisar_lista_de_parametros(parser);
        if(parser->em_recuperacao){
            sincronizar_ate(parser, TOKEN_RPAREN);
        }
    }

    consumir_token(parser, TOKEN_RPAREN);
    ASTNode *body = analisar_bloco(parser);

    ASTNode *func = ast_new_node(AST_FUNCTION_DECL, line, column);
    func->name = name;
    func->value_type = return_type;
    func->left = params;
    func->right = body;
    return func;
}

ASTNode *analisar_programa(Parser *parser){
    ASTNode *root = ast_new_node(AST_PROGRAM,
                                parser->current_token.line,
                                parser->current_token.column);
    root->left = analisar_lista_de_funcoes(parser);
    consumir_token(parser, TOKEN_EOF);
    return root;
}
