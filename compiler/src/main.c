#include "lexer_v1.h"
#include "parser.h"
#include "ast.h"

char *ler_arquivo(const char *filename);

int main(int argc, char *argv[]) {
    Lexer lexer;
    Parser parser;

    const char *filename = (argc > 1) ? argv[1] : "compiler/tests/test_ast.txt";
    char *source = ler_arquivo(filename);
    if (source == NULL) {
        printf("Erro ao abrir o arquivo: %s\n", filename);
        return 1;
    }

    inicializar_lexer(&lexer, source);
    inicializar_parser(&parser, &lexer);

    ASTNode *ast = analisar_programa(&parser);

    printf("\n");

    if(parser.quantidade_erros == 0){

        printf(GREEN
            "====================================\n"
            RESET);

        printf(GREEN
            "ANÁLISE SINTÁTICA CONCLUÍDA\n"
            RESET);

        printf(GREEN
            "Nenhum erro encontrado.\n"
            RESET);

        printf(GREEN
            "====================================\n"
            RESET);

        if(ast){
            printf(GREEN "AST gerado:\n" RESET);
            ast_print(ast, 0);
        }

    } else {

        printf(RED
            "====================================\n"
            RESET);

        printf(RED
            "ANÁLISE SINTÁTICA FINALIZADA\n"
            RESET);

        printf(RED
            "Erros encontrados: %d\n",
            parser.quantidade_erros);

        printf(RED
            "====================================\n"
            RESET);
    }
    
    ast_free(ast);
    free(source);
    return 0;
}