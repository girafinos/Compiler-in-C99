#include "lexer_v1.h"

char *ler_arquivo(const char *filename);

int main(int argc, char *argv[]) {
    Lexer lexer;
    Token token;

    const char *filename = (argc > 1) ? argv[1] : "compiler/tests/entrada.txt";
    char *source = ler_arquivo(filename);
    if (source == NULL) {
        printf("Erro ao abrir o arquivo: %s\n", filename);
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