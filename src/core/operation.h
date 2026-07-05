#ifndef CORE_OPERATION_H
#define CORE_OPERATION_H

#include <stdint.h>

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

#define OPERATION_COUNT 16

/*
  The SHARED identity of an LC-3 operation: its opcode and its canonical
  mnemonic, and nothing else. This is the "base class" of the design — defined
  exactly once here, in the core both the virtual machine and the assembler
  link against. Each usage context wraps this with its own behavior:
    - the VM adds `execute`  (VirtualMachineOperation, in virtual_machine/)
    - the assembler adds `encode` (AssemblyOperation, in assembler/)
  A future disassembler would add `decode`, touching none of the above.
*/
typedef struct Operation {
  uint16_t code;      /* bits 15-12; also its index into OPERATIONS */
  const char *name;   /* canonical mnemonic, e.g. "ADD" */
} Operation;

extern const Operation OPERATION_BR;    /* branch */
extern const Operation OPERATION_ADD;   /* add */
extern const Operation OPERATION_LD;    /* load */
extern const Operation OPERATION_ST;    /* store */
extern const Operation OPERATION_JSR;   /* jump register */
extern const Operation OPERATION_AND;   /* bitwise and */
extern const Operation OPERATION_LDR;   /* load register */
extern const Operation OPERATION_STR;   /* store register */
extern const Operation OPERATION_RTI;   /* return from interrupt (unused) */
extern const Operation OPERATION_NOT;   /* bitwise not */
extern const Operation OPERATION_LDI;   /* load indirect */
extern const Operation OPERATION_STI;   /* store indirect */
extern const Operation OPERATION_JMP;   /* jump */
extern const Operation OPERATION_RES;   /* reserved (unused) */
extern const Operation OPERATION_LEA;   /* load effective address */
extern const Operation OPERATION_TRAP;  /* execute trap */

extern const Operation *const OPERATIONS[OPERATION_COUNT];

const Operation *operation_by_code(uint16_t code);
const Operation *operation_by_name(const char *name);

#endif /* CORE_OPERATION_H */
