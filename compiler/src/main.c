#include "lexer_v1.h"
#include "parser.h"

char *ler_arquivo(const char *filename);

int main(int argc, char *argv[]) {
    Lexer lexer;
    Parser parser;

    const char *filename = (argc > 1) ? argv[1] : "compiler/tests/entrada.txt";
    char *source = ler_arquivo(filename);
    if (source == NULL) {
        printf("Erro ao abrir o arquivo: %s\n", filename);
        return 1;
    }

    inicializar_lexer(&lexer, source);
    inicializar_parser(&parser, &lexer);

    come(&parser, TOKEN_INT);
    come(&parser, TOKEN_ID);
    come(&parser, TOKEN_SEMICOLON);

    printf("Teste do parser concluido com sucesso.\n");

    free(source);
    return 0;
}