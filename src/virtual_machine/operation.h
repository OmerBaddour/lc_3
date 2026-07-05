#ifndef VIRTUAL_MACHINE_OPERATION_H
#define VIRTUAL_MACHINE_OPERATION_H

#include <stdint.h>
#include "core/operation.h"

/*
  The virtual machine's "subclass" of Operation: it points at the shared base
  (for its code + name) and adds the runtime behavior — `execute`. The VM never
  needs the assembler's `encode`, and vice versa; each side wraps the same base.

  Every execute takes the uniform (instruction, registers, memory) signature so
  they share one function-pointer type. Register-only operations simply ignore
  `memory`.
*/
typedef struct VirtualMachineOperation {
  const Operation *operation;
  void (*execute)(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
} VirtualMachineOperation;

/* Indexed by opcode; entries for opcodes with no runtime behavior (RES) are NULL. */
extern const VirtualMachineOperation *const VIRTUAL_MACHINE_OPERATIONS[OPERATION_COUNT];

const VirtualMachineOperation *virtual_machine_operation_by_code(uint16_t code);

/* the execute implementations (uniform signature; register-only ops ignore memory) */
void operation_add(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_and(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_not(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_br(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_jmp(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_jsr(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_lea(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_ld(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_ldi(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_ldr(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_st(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_sti(uint16_t instruction, uint16_t registers[], uint16_t memory[]);
void operation_str(uint16_t instruction, uint16_t registers[], uint16_t memory[]);

#endif /* VIRTUAL_MACHINE_OPERATION_H */
