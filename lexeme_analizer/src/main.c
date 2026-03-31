#include "lexer_v1.h"

char *ler_arquivo(const char *filename);

int main() {
    Lexer lexer;
    Token token;

    char *source = ler_arquivo("entrada.txt");
    if (source == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return 1;
    }

    inicializar_lexer(&lexer, source);

    do {
        token = pegar_prox_token(&lexer);
        print_token(token);
    } while (token.type != TOKEN_EOF);

    free(source);
    return 0;
}