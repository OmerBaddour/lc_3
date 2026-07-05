#ifndef ASSEMBLER_OPERATION_H
#define ASSEMBLER_OPERATION_H

#include <stdint.h>
#include "core/operation.h"
#include "assembler/assembler.h"      /* AssemblyInstruction, Operand */
#include "assembler/symbol_table.h"   /* SymbolTable */

/*
  The assembler's "subclass" of Operation: it points at the shared base (for its
  code + name) and adds `encode` — the mirror of the VM's `execute`. Given a
  parsed instruction line and the symbol table, encode produces the 16-bit
  machine word, setting *ok = 0 on any error (unresolved label, out-of-range
  immediate, ...).
*/
typedef struct AssemblyOperation {
  const Operation *operation;
  uint16_t (*encode)(const AssemblyInstruction *line, const SymbolTable *table, int *ok);
} AssemblyOperation;

/* Indexed by opcode; OP_RES has no encoding and is NULL. */
extern const AssemblyOperation *const ASSEMBLY_OPERATIONS[OPERATION_COUNT];

/* Resolve a mnemonic (already upper-cased) to its AssemblyOperation, handling
   the mnemonic families the VM never sees: the BR condition-code suffixes
   (BR, BRn, ... BRnzp), RET (= JMP R7), and JSRR (= register-mode JSR). Returns
   NULL for anything that is not an operation (e.g. a trap alias or a directive). */
const AssemblyOperation *assembly_operation_by_name(const char *upper_name);

#endif /* ASSEMBLER_OPERATION_H */
