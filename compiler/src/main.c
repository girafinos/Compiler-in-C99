#include "lexer_v1.h"
#include "parser.h"

char *ler_arquivo(const char *filename);

int main(int argc, char *argv[]) {
    Lexer lexer;
    Parser parser;
    CodeGen cg;

    const char *filename = (argc > 1) ? argv[1] : "compiler/tests/ok.txt";
    char *source = ler_arquivo(filename);
    if (source == NULL) {
        printf("Erro ao abrir o arquivo: %s\n", filename);
        return 1;
    }

    codegen_init(&cg);
    inicializar_lexer(&lexer, source);
    inicializar_parser(&parser, &lexer, &cg);

    analisar_programa(&parser);

    printf("\n");

    if(parser.quantidade_erros == 0){

        printf(GREEN
            "====================================\n"
            RESET);

        printf(GREEN
            "ANÁLISE SINTÁTICA E SEMÂNTICA CONCLUÍDA\n"
            RESET);

        printf(GREEN
            "Nenhum erro encontrado.\n"
            RESET);

        printf(GREEN
            "====================================\n"
            RESET);

    } else {

        printf(RED
            "====================================\n"
            RESET);

        printf(RED
            "ANÁLISE SINTÁTICA E SEMÂNTICA FINALIZADA\n"
            RESET);

        printf(RED
            "Erros encontrados: %d\n",
            parser.quantidade_erros);

        printf(RED
            "====================================\n"
            RESET);
    }
    
    free(source);
    return parser.quantidade_erros == 0 ? 0 : 1;
}