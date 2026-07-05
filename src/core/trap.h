#ifndef CORE_TRAP_H
#define CORE_TRAP_H

#include <stdint.h>

#define TRAP_COUNT 6

/*
  The SHARED identity of an LC-3 trap: its vector and its canonical mnemonic,
  and nothing else. This is the "base class", mirroring core/operation.c's
  Operation — defined once in the core the virtual machine and assembler both
  link against. The VM wraps this with `execute` (VirtualMachineTrap, in
  virtual_machine/); the assembler needs only the base (name -> vector), so it
  reuses this directly.

  Unlike opcodes, trap vectors do NOT start at 0 or run contiguously (they begin
  at 0x20), so the code is not an array index — lookups scan (trap_by_code).
*/
typedef struct Trap {
  uint16_t code;      /* trap vector; goes in bits 7-0 of a TRAP instruction */
  const char *name;   /* canonical mnemonic, e.g. "GETC" */
} Trap;

extern const Trap TRAP_GETC;   /* get character from keyboard, not echoed */
extern const Trap TRAP_OUT;    /* output a character */
extern const Trap TRAP_PUTS;   /* output a word string */
extern const Trap TRAP_IN;     /* get character from keyboard, echoed */
extern const Trap TRAP_PUTSP;  /* output a byte string */
extern const Trap TRAP_HALT;   /* halt the program */

extern const Trap *const TRAPS[TRAP_COUNT];

const Trap *trap_by_code(uint16_t code);
const Trap *trap_by_name(const char *name);

#endif /* CORE_TRAP_H */
