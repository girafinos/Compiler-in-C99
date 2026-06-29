#include "codegen.h"
#include <stdio.h>
#include <stdarg.h>

void codegen_init(CodeGen *cg) {
    cg->reg_counter   = 0;
    cg->label_counter = 0;
}

int codegen_novo_reg(CodeGen *cg) {
    if (cg->reg_counter >= MAX_TEMP_REGS) {
        fprintf(stderr, "[CODEGEN] Registradores temporários esgotados\n");
        return MAX_TEMP_REGS - 1;
    }
    return cg->reg_counter++;
}

void codegen_resetar_regs(CodeGen *cg) {
    cg->reg_counter = 0;
}

int codegen_novo_label(CodeGen *cg) {
    return cg->label_counter++;
}

void codegen_emitir(const char *fmt, ...) {
    printf("    ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void codegen_emitir_label(const char *nome) {
    printf("%s:\n", nome);
}