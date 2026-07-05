#ifndef VIRTUAL_MACHINE_TRAP_H
#define VIRTUAL_MACHINE_TRAP_H

#include <stdint.h>
#include <stdio.h>
#include "core/trap.h"

/*
  The virtual machine's "subclass" of Trap: it points at the shared base (for its
  code + name) and adds the runtime behavior — `execute`. Mirrors
  VirtualMachineOperation.

  Every execute takes the uniform (memory, registers, input, output) signature so
  they share one function-pointer type; a trap ignores whichever parameters it
  does not need (GETC reads only `input`, OUT writes only `output`, ...). HALT has
  no behavior of its own — it must stop the fetch loop, which only lc_3.c can do —
  so its execute is NULL, exactly as OP_RES is NULL in the operation table.
*/
typedef struct VirtualMachineTrap {
  const Trap *trap;
  void (*execute)(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output);
} VirtualMachineTrap;

extern const VirtualMachineTrap *const VIRTUAL_MACHINE_TRAPS[TRAP_COUNT];

const VirtualMachineTrap *virtual_machine_trap_by_code(uint16_t code);

/* the execute implementations (uniform signature; each ignores unused params) */
void trap_getc(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output);
void trap_out(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output);
void trap_puts(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output);
void trap_in(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output);
void trap_putsp(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output);

#endif /* VIRTUAL_MACHINE_TRAP_H */
