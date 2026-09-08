// codegen.h
#ifndef CODEGEN_H
#define CODEGEN_H

#include<stdlib.h>

#define MAX_TEMP_REGS 10

typedef struct {
    char  *buf;       // buffer de texto acumulado
    size_t len;        // bytes usados
    size_t cap;        // capacidade alocada
} CodeBuffer;

typedef struct CodeGen {
    int reg_counter;
    int label_counter;
    int var_counter;
    CodeBuffer data;   // segmento .data
    CodeBuffer text;   // segmento .text
} CodeGen;

void codegen_init(CodeGen *cg);
void codegen_destruir(CodeGen *cg);
int  codegen_novo_reg(CodeGen *cg);
void codegen_resetar_regs(CodeGen *cg);
int  codegen_novo_label(CodeGen *cg);
// Mantêm a mesma assinatura usada no parser.c — agora gravam no buffer .text
void codegen_emitir(CodeGen *cg, const char *fmt, ...);
void codegen_emitir_label(CodeGen *cg, const char *nome);
// Nova: registra uma variável global no segmento .data
void codegen_registrar_global(CodeGen *cg, const char *nome, int tamanho_palavras);
// Imprime .data seguido de .text, na ordem certa, ao final da compilação
void codegen_finalizar(CodeGen *cg);

#endif