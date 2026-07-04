#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * PARKED: assembler-side IR + Operation-table experiment (mid-refactor).
 * Wrapped in `#if 0` (not a block comment) because the code below contains
 * its own /* ... *​/ comments, which would prematurely close a block comment.
 * `#if 0 ... #endif` is the idiomatic C way to disable a span that itself
 * contains comments. The VM doesn't need any of this — it dispatches via the
 * switch in lc_3.c over the free `operation_*` functions declared further down.
 * ------------------------------------------------------------------------- */
#if 0

typedef enum {
    OPERAND_REGISTER,
    OPERAND_INTEGER,
    OPERAND_LABEL
} OperandType;

typedef struct Operand {
    OperandType type;
    union {
        const Register *reg;
        int integer;
        char *label;
    } data;
} Operand;

typedef struct OpcodeInstruction {
    char *op_code;
    Operand operand[];
} OpcodeInstruction;

typedef struct InstructionIntermediary {
    const char *raw_instruction;
    uint16_t address;             /* relative to .ORIG */
    const char *label;            /* label (if defined, else NULL) */
    const Operation *operation;
    Operand operands[3];          /* max is 3 */
    int operands_length;          /* length of operands list */
    uint16_t *machine_codes;      /* machine codes to emit */
    int machine_codes_length;     /* length of machine codes */
} InstructionIntermediary;

typedef struct SymbolTableRecord {
    char *label;
    InstructionIntermediary *instruction_intermediary;
} SymbolTableRecord;

typedef struct Operation {
    const uint16_t code;
    const char *name;
    void (*execute)(const uint16_t instruction, uint16_t registers[], uint16_t memory[]);
    AssemblyRaw (*assemble_raw)(const Operation *self, const char *operands);
    uint16_t (*assemble_symbol_table)(const Operation *self, const char *operands, const SymbolTableRecord *symbol_table);
} Operation;

extern const Operation OPERATION_BR;   /* branch */
extern const Operation OPERATION_ADD;  /* add */
extern const Operation OPERATION_LD;   /* load */
extern const Operation OPERATION_ST;   /* store */
extern const Operation OPERATION_JSR;  /* jump register */
extern const Operation OPERATION_AND;  /* bitwise and */
extern const Operation OPERATION_LDR;  /* load register */
extern const Operation OPERATION_STR;  /* store register */
extern const Operation OPERATION_RTI;  /* unused */
extern const Operation OPERATION_NOT;  /* bitwise not */
extern const Operation OPERATION_LDI;  /* load indirect */
extern const Operation OPERATION_STI;  /* store indirect */
extern const Operation OPERATION_JMP;  /* jump */
extern const Operation OPERATION_RES;  /* reserved (unused) */
extern const Operation OPERATION_LEA;  /* load effective address */
extern const Operation OPERATION_TRAP;  /* execute trap */

#endif /* PARKED assembler-side IR + Operation-table experiment */

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
