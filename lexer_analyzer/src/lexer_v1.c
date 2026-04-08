#include "lexer_v1.h"

void inicializar_lexer(Lexer *lexer, const char *source){
    lexer->src = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;

    if (source[0] != '\0'){
        lexer->current_char = source[0];
    } else {
        lexer->current_char = '\0';
    }
}   

void andar_char(Lexer *lexer){
    if (lexer->current_char != '\0'){
        if(lexer->current_char == '\n'){
            lexer->line++;
            lexer->column = 1;
        } else {
            lexer->column++;
        }

        lexer->pos++;

        if(lexer->src[lexer->pos] != '\0'){
            lexer->current_char = lexer->src[lexer->pos];
        } else {
            lexer->current_char = '\0';
        }
    } else {
        return;
    }
}

char spoiler_prox_char(Lexer *lexer){
    if(lexer->src[lexer->pos + 1] != '\0'){
        return lexer->src[lexer->pos + 1];
    } else {
        return '\0';
    }
}

void pular_espacos(Lexer *lexer){

//     while(lexer->current_char == ' ' || lexer->current_char == '\t' || lexer->current_char == '\n' || lexer->current_char == '\r'){
//         advance_char(lexer);
//     }

    while(lexer->current_char != '\0' && isspace(lexer->current_char)){
        andar_char(lexer);   
    }
}

Token cria_token(TokenType type, const char* lexema, int line, int column){
    Token token;
    token.type = type;
    strcpy(token.lexema, lexema);
    token.line = line;
    token.column = column;
    return token;
}

TokenType palavra_chave_ou_id(const char *lexema){
    if(strcmp(lexema, "int") == 0) return TOKEN_INT;
    if(strcmp(lexema, "char") == 0) return TOKEN_CHAR;
    if(strcmp(lexema, "const") == 0) return TOKEN_CONST;
    if(strcmp(lexema, "void") == 0) return TOKEN_VOID;
    if(strcmp(lexema, "if") == 0) return TOKEN_IF;
    if(strcmp(lexema, "else") == 0) return TOKEN_ELSE;
    if(strcmp(lexema, "while") == 0) return TOKEN_WHILE;
    if(strcmp(lexema, "return") == 0) return TOKEN_RETURN;
    if(strcmp(lexema, "switch") == 0) return TOKEN_SWITCH;
    if(strcmp(lexema, "case") == 0) return TOKEN_CASE;
    if(strcmp(lexema, "default") == 0) return TOKEN_DEFAULT;
    if(strcmp(lexema, "typedef") == 0) return TOKEN_TYPEDEF;
    if(strcmp(lexema, "struct") == 0) return TOKEN_STRUCT;
    if(strcmp(lexema, "enum") == 0) return TOKEN_ENUM;
    if(strcmp(lexema, "break") == 0) return TOKEN_BREAK;

    return TOKEN_ID; // Se não for uma palavra reservada, é um identificador
}

Token identificadores(Lexer *lexer){
    char buffer[100];
    int i = 0;
    int start_line = lexer->line;
    int start_column = lexer->column; 

    while((lexer->current_char != '\0') && (isalnum(lexer->current_char) || lexer->current_char == '_') && i<99){
        buffer[i] = lexer->current_char;
        i++;
        andar_char(lexer);
    }

    buffer[i] = '\0';

    return cria_token(palavra_chave_ou_id(buffer), buffer, start_line, start_column);
}

Token numeros(Lexer *lexer){
    char buffer[100];
    int i = 0;
    int start_line = lexer->line;
    int start_column = lexer->column; 

    while(lexer->current_char != '\0' && isdigit(lexer->current_char) && i<99){
        buffer[i] = lexer->current_char;
        i++;
        andar_char(lexer);
    }

    buffer[i] = '\0';

    return cria_token(TOKEN_NUM, buffer, start_line, start_column);
}

Token pegar_prox_token(Lexer *lexer){
    while(lexer->current_char != '\0'){
        if(isspace(lexer->current_char)){
            pular_espacos(lexer);
            continue;
        }
        if(isalpha(lexer->current_char) || lexer->current_char == '_'){
            return identificadores(lexer);
        }
        if(isdigit(lexer->current_char)){
            return numeros(lexer);
        }

        //Operadores
        if(lexer->current_char == '+'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_PLUS, "+", start_line, start_column);
        }
        if(lexer->current_char == '-'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_MINUS, "-", start_line, start_column); 
        }
        if(lexer->current_char == '='){
            int start_line = lexer->line;
            int start_column = lexer->column;
            if(spoiler_prox_char(lexer) == '='){
                andar_char(lexer);
                andar_char(lexer);
                return cria_token(TOKEN_EQ, "==", start_line, start_column);
            }
            andar_char(lexer);
            return cria_token(TOKEN_ASSIGN, "=", start_line, start_column);
        }
        if(lexer->current_char == '*'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_MULT, "*", start_line, start_column);
        }
        if(lexer->current_char == '/'){
            if(spoiler_prox_char(lexer) == '/'){
                andar_char(lexer);
                andar_char(lexer);
                pular_comentario_linha(lexer);
                continue;
            }
            if(spoiler_prox_char(lexer) == '*'){
                andar_char(lexer);
                andar_char(lexer);
                pular_comentario_bloco(lexer);
                continue;
            }

            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_DIV, "/", start_line, start_column);
        }
        if(lexer->current_char == '<'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            if(spoiler_prox_char(lexer) == '='){
                andar_char(lexer);
                andar_char(lexer);
                return cria_token(TOKEN_LTE, "<=", start_line, start_column);
            }
            andar_char(lexer);
            return cria_token(TOKEN_LT, "<", start_line, start_column);
        }
        if(lexer->current_char == '>'){
            int start_line = lexer->line;
            int start_column = lexer->column;  
            if(spoiler_prox_char(lexer) == '='){
                andar_char(lexer);
                andar_char(lexer);
                return cria_token(TOKEN_GTE, ">=", start_line, start_column);
            }
            andar_char(lexer);
            return cria_token(TOKEN_GT, ">", start_line, start_column);
        }
        if(lexer->current_char == '!'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            if(spoiler_prox_char(lexer) == '='){
                andar_char(lexer);
                andar_char(lexer);
                return cria_token(TOKEN_NEQ, "!=", start_line, start_column);
            }
            andar_char(lexer);
            return cria_token(TOKEN_NOT, "!", start_line, start_column);
        }

        //Caracteres especiais
        if(lexer->current_char == '#'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_HASH, "#", start_line, start_column);
        }
        if(lexer->current_char == '.'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_DOT, ".", start_line, start_column);
        }
        if(lexer->current_char == '&'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            if(spoiler_prox_char(lexer) == '&'){
                andar_char(lexer);
                andar_char(lexer);
                return cria_token(TOKEN_AND, "&&", start_line, start_column);
            }
            andar_char(lexer);
            return cria_token(TOKEN_AMPERSAND, "&", start_line, start_column);
        }
        if(lexer->current_char == '|'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            if(spoiler_prox_char(lexer) == '|'){
                andar_char(lexer);
                andar_char(lexer);
                return cria_token(TOKEN_OR, "||", start_line, start_column);
            }
            andar_char(lexer);
            return cria_token(TOKEN_PIPE, "|", start_line, start_column);
        }

        //Delimitadores
        if(lexer->current_char == ';'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_SEMICOLON, ";", start_line, start_column);
        }
        if(lexer->current_char == ','){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_COMMA, ",", start_line, start_column);
        }
        if(lexer->current_char == '('){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_LPAREN, "(", start_line, start_column);
        }
        if(lexer->current_char == ')'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_RPAREN, ")", start_line, start_column);
        }
        if(lexer->current_char == '{'){
            int start_line = lexer->line;   
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_LBRACE, "{", start_line, start_column);
        }
        if(lexer->current_char == '}'){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_RBRACE, "}", start_line, start_column);
        }
        if(lexer->current_char == '['){
            int start_line = lexer->line;
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_LBRACKET, "[", start_line, start_column);
        }
        if(lexer->current_char == ']'){
            int start_line = lexer->line;   
            int start_column = lexer->column;
            andar_char(lexer);
            return cria_token(TOKEN_RBRACKET, "]", start_line, start_column);
        }
        if(lexer)
        {
            char error_lexema[2];
            int start_line = lexer->line;
            int start_column = lexer->column;

            error_lexema[0] = lexer->current_char;
            error_lexema[1] = '\0';

            andar_char(lexer);
            return cria_token(TOKEN_ERROR, error_lexema, start_line, start_column);

        }
    }
    return cria_token(TOKEN_EOF, "EOF", lexer->line, lexer->column);
}

void pular_comentario_linha(Lexer *lexer){
    while(lexer->current_char != '\0' && lexer->current_char != '\n'){
        andar_char(lexer);  
    }
}   

void pular_comentario_bloco(Lexer *lexer){
    while(lexer->current_char != '\0'){
        if(lexer->current_char == '*' && spoiler_prox_char(lexer) == '/'){
            andar_char(lexer);
            andar_char(lexer);
            break;
        }
        andar_char(lexer);
    }
} 

const char* token_para_string(TokenType type){
    switch(type){
        case TOKEN_ID: return "TOKEN_ID";
        case TOKEN_NUM: return "TOKEN_NUM";

        case TOKEN_INT: return "TOKEN_INT";
        case TOKEN_CHAR: return "TOKEN_CHAR";
        case TOKEN_CONST: return "TOKEN_CONST";
        case TOKEN_VOID: return "TOKEN_VOID";
        case TOKEN_IF: return "TOKEN_IF";
        case TOKEN_ELSE: return "TOKEN_ELSE";
        case TOKEN_WHILE: return "TOKEN_WHILE";
        case TOKEN_RETURN: return "TOKEN_RETURN";
        case TOKEN_SWITCH: return "TOKEN_SWITCH";
        case TOKEN_CASE: return "TOKEN_CASE";
        case TOKEN_DEFAULT: return "TOKEN_DEFAULT";
        case TOKEN_TYPEDEF: return "TOKEN_TYPEDEF";
        case TOKEN_STRUCT: return "TOKEN_STRUCT";
        case TOKEN_ENUM: return "TOKEN_ENUM";
        case TOKEN_BREAK: return "TOKEN_BREAK";

        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_MULT: return "TOKEN_MULT";
        case TOKEN_DIV: return "TOKEN_DIV";
        case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
        case TOKEN_LT: return "TOKEN_LT";
        case TOKEN_GT: return "TOKEN_GT";
        case TOKEN_NOT: return "TOKEN_NOT";

        case TOKEN_HASH: return "TOKEN_HASH";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_AMPERSAND: return "TOKEN_AMPERSAND";
        case TOKEN_PIPE: return "TOKEN_PIPE";

        case TOKEN_EQ: return "TOKEN_EQ";
        case TOKEN_NEQ: return "TOKEN_NEQ"; 
        case TOKEN_LTE: return "TOKEN_LTE";
        case TOKEN_GTE: return "TOKEN_GTE";
        case TOKEN_AND: return "TOKEN_AND";
        case TOKEN_OR: return "TOKEN_OR";

        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_COMMA: return "TOKEN_COMMA";

        case TOKEN_LPAREN: return "TOKEN_LPAREN";
        case TOKEN_RPAREN: return "TOKEN_RPAREN";
        case TOKEN_LBRACE: return "TOKEN_LBRACE";
        case TOKEN_RBRACE: return "TOKEN_RBRACE";
        case TOKEN_LBRACKET: return "TOKEN_LBRACKET";
        case TOKEN_RBRACKET: return "TOKEN_RBRACKET";

        case TOKEN_EOF: return "TOKEN_EOF";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        default: return "TOKEN_UNKNOWN";
    }
}

void print_token(Token token){
    printf("Token -> type: %s | lexema: \"%s\" | line: %d | column: %d\n",
           token_para_string(token.type),
           token.lexema,
           token.line,
           token.column);
}

char *ler_arquivo(const char *filename){
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, size, file);
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}