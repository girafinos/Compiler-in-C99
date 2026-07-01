#include "codegen.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_INITIAL_CAP 1024

// -----------------------------------------------------------------------------
//  Buffer
// -----------------------------------------------------------------------------

static void buffer_init(CodeBuffer *b) {
    b->cap = BUFFER_INITIAL_CAP;
    b->len = 0;
    b->buf = malloc(b->cap);
    b->buf[0] = '\0';
}

static void buffer_destruir(CodeBuffer *b) {
    free(b->buf);
    b->buf = NULL;
    b->len = b->cap = 0;
}

// Garante espaço para mais `extra` bytes (+1 para o terminador nulo)
static void buffer_reservar(CodeBuffer *b, size_t extra) {
    size_t necessario = b->len + extra + 1;
    if (necessario <= b->cap) return;

    size_t nova_cap = b->cap;
    while (nova_cap < necessario) nova_cap *= 2;

    char *novo = realloc(b->buf, nova_cap);
    if (!novo) {
        fprintf(stderr, "[CODEGEN] Falha ao realocar buffer\n");
        exit(1);
    }
    b->buf = novo;
    b->cap = nova_cap;
}

static void buffer_append(CodeBuffer *b, const char *s) {
    size_t s_len = strlen(s);
    buffer_reservar(b, s_len);
    memcpy(b->buf + b->len, s, s_len + 1); // copia o '\0' também
    b->len += s_len;
}

static void buffer_appendf(CodeBuffer *b, const char *fmt, va_list args) {
    // Primeiro tenta estimar o tamanho necessário com vsnprintf(NULL, 0, ...)
    va_list args_copy;
    va_copy(args_copy, args);
    int precisa = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (precisa < 0) return;

    buffer_reservar(b, (size_t)precisa);
    vsnprintf(b->buf + b->len, (size_t)precisa + 1, fmt, args);
    b->len += (size_t)precisa;
}

// -----------------------------------------------------------------------------
//  público
// -----------------------------------------------------------------------------

void codegen_init(CodeGen *cg) {
    cg->reg_counter   = 0;
    cg->label_counter = 0;
    cg->var_counter = 0;
    buffer_init(&cg->data);
    buffer_init(&cg->text);
}

void codegen_destruir(CodeGen *cg) {
    buffer_destruir(&cg->data);
    buffer_destruir(&cg->text);
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

void codegen_emitir(CodeGen *cg, const char *fmt, ...) {
    buffer_append(&cg->text, "    ");
    va_list args;
    va_start(args, fmt);
    buffer_appendf(&cg->text, fmt, args);
    va_end(args);
    buffer_append(&cg->text, "\n");
}

void codegen_emitir_label(CodeGen *cg, const char *nome) {
    buffer_append(&cg->text, nome);
    buffer_append(&cg->text, ":\n");
}

void codegen_registrar_global(CodeGen *cg, const char *nome, int tamanho_palavras) {
    char linha[256];
    // .word 0, 0, ... — uma palavra zerada por slot (1 para escalares)
    snprintf(linha, sizeof(linha), "%s: .word 0", nome);
    for (int i = 1; i < tamanho_palavras; i++) {
        strncat(linha, ", 0", sizeof(linha) - strlen(linha) - 1);
    }
    strncat(linha, "\n", sizeof(linha) - strlen(linha) - 1);
    buffer_append(&cg->data, linha);
}

void codegen_finalizar(CodeGen *cg) {
    printf(".data\n");
    if (cg->data.len > 0) printf("%s", cg->data.buf);
    printf("\n.text\n");
    if (cg->text.len > 0) printf("%s", cg->text.buf);
}