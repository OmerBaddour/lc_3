#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include "core/registers.h"   /* for `Register` and `register_by_name` */

/*
  An operand is *exactly one of* a few things: a register (R0), an integer
  immediate (#5, x1F), a label reference (LOOP), or a string literal
  ("Hello"). Python would call this a Union; C expresses "one of" with a
  TAGGED UNION:

    - a `type` tag  -> which kind is this?
    - a `union`     -> storage shared by all members; ONLY the member named by
                       the tag is valid to read. The union is exactly as big as
                       its largest member, not the sum — that is the whole point
                       vs. separate fields per kind.
*/
typedef enum {
  OPERAND_REGISTER,  /* R0..R7          -> use `as.reg`     */
  OPERAND_INTEGER,   /* #5, x1F, 42     -> use `as.integer` */
  OPERAND_LABEL,     /* LOOP, HELLO     -> use `as.label`   */
  OPERAND_STRING     /* "Hello World!"  -> use `as.string`  */
} OperandType;

typedef struct Operand {
  OperandType type;
  union {
    const Register *reg;  /* points into the shared REGISTERS table; NOT owned */
    int integer;
    char *label;          /* symbolic name, heap-allocated, OWNED by this Operand */
    char *string;         /* .STRINGZ literal text, heap-allocated, OWNED       */
  } as;
} Operand;

/*
  One source line, parsed but not yet turned into machine code. This is the
  "intermediary" between text and bits. Pass 1 fills in the parse fields;
  `address` and `machine_codes` get filled in later passes.
*/
#define MAX_OPERANDS 3   /* the widest LC-3 instruction (e.g. ADD DR, SR1, SR2) */

typedef struct AssemblyInstruction {
  char *raw;                 /* original source line, OWNED — priceless for error messages */
  char *label;               /* label DEFINED on this line, or NULL. OWNED */
  char *operation_name;      /* "ADD", ".STRINGZ", ... OWNED. (Later: resolve to const Operation*) */
  Operand operands[MAX_OPERANDS];
  int operands_length;

  uint16_t address;          /* set in pass 1 by the location counter (0 for now) */
  uint16_t *machine_codes;   /* set in pass 2; NULL until then. OWNED */
  int machine_codes_length;
} AssemblyInstruction;

/* Parse one line of assembly. Returns NULL for blank / comment-only lines. */
AssemblyInstruction *parse_assembly_line(const char *line);

/* Free an AssemblyInstruction and everything it owns. */
void free_assembly_instruction(AssemblyInstruction *instruction);

/* Debug: print the parsed form so we can watch pass 1 work. */
void print_assembly_instruction(const AssemblyInstruction *instruction);

/* Two passes over the whole program: pass 1 assigns addresses and builds the
   symbol table, pass 2 emits machine_codes on every line. Writes the origin
   to *origin_out. Returns 1 on success, 0 if any line failed to assemble. */
int assemble(AssemblyInstruction **program, int count, uint16_t *origin_out);

/* Dump an assembled program: origin word (big-endian), then every line's
   machine_codes in order. Returns 1 on success, 0 if the file can't be opened. */
int write_object_file(const char *path, uint16_t origin,
                      AssemblyInstruction **program, int count);

#endif /* ASSEMBLER_H */
