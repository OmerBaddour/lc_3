#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stdint.h>

typedef struct Operation {
    const uint16_t code;
    const char *name;
    void (*execute)(const uint16_t instruction, uint16_t registers[], uint16_t memory[]);
    uint16_t (*assemble)(const struct Operation *self, const char *operands);
};

extern const struct Operation OPERATION_BR;   /* branch */
extern const struct Operation OPERATION_ADD;  /* add */
extern const struct Operation OPERATION_LD;   /* load */
extern const struct Operation OPERATION_ST;   /* store */
extern const struct Operation OPERATION_JSR;  /* jump register */
extern const struct Operation OPERATION_AND;  /* bitwise and */
extern const struct Operation OPERATION_LDR;  /* load register */
extern const struct Operation OPERATION_STR;  /* store register */
extern const struct Operation OPERATION_RTI;  /* unused */
extern const struct Operation OPERATION_NOT;  /* bitwise not */
extern const struct Operation OPERATION_LDI;  /* load indirect */
extern const struct Operation OPERATION_STI;  /* store indirect */
extern const struct Operation OPERATION_JMP;  /* jump */
extern const struct Operation OPERATION_RES;  /* reserved (unused) */
extern const struct Operation OPERATION_LEA;  /* load effective address */
extern const struct Operation OPERATION_TRAP;  /* execute trap */

/* operation opcodes (bits 15-12 of an instruction) */
enum {
  OP_BR = 0,  /* branch */
  OP_ADD,     /* add */
  OP_LD,      /* load */
  OP_ST,      /* store */
  OP_JSR,     /* jump register */
  OP_AND,     /* bitwise and */
  OP_LDR,     /* load register */
  OP_STR,     /* store register */
  OP_RTI,     /* unused */
  OP_NOT,     /* bitwise not */
  OP_LDI,     /* load indirect */
  OP_STI,     /* store indirect */
  OP_JMP,     /* jump */
  OP_RES,     /* reserved (unused) */
  OP_LEA,     /* load effective address */
  OP_TRAP     /* execute trap */
};

uint16_t sign_extend(uint16_t x, int bit_count);

/* register-only operations */
void operation_add(uint16_t instruction, uint16_t registers[]);
void operation_and(uint16_t instruction, uint16_t registers[]);
void operation_not(uint16_t instruction, uint16_t registers[]);
void operation_br(uint16_t instruction, uint16_t registers[]);
void operation_jmp(uint16_t instruction, uint16_t registers[]);
void operation_jsr(uint16_t instruction, uint16_t registers[]);
void operation_lea(uint16_t instruction, uint16_t registers[]);

/* memory-reading operations */
void operation_ld(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_ldi(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_ldr(uint16_t instruction, uint16_t registers[], uint16_t memory[]);

/* memory-writing operations */
void operation_st(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_sti(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_str(uint16_t instruction, uint16_t registers[], uint16_t memory[]);

#endif /* OPERATIONS_H */
