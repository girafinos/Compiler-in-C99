#ifndef CODEGEN_H
#define CODEGEN_H

#define MAX_TEMP_REGS 10

typedef struct {
    int reg_counter;
    int label_counter;
} CodeGen;

void codegen_init(CodeGen *cg);
int  codegen_novo_reg(CodeGen *cg);
void codegen_resetar_regs(CodeGen *cg);
int  codegen_novo_label(CodeGen *cg);
void codegen_emitir(const char *fmt, ...);
void codegen_emitir_label(const char *nome);

#endif