#ifndef DISASSEMBLER_OPERATION_H
#define DISASSEMBLER_OPERATION_H

#include <stdint.h>
#include "core/operation.h"
#include "disassembler/disassembler.h"   /* DisassemblyInstruction */

/*
  The disassembler's "subclass" of Operation: it points at the shared base (for
  its code + name) and adds `decode` — the mirror of the assembler's `encode`.
  Given a program word and the address it lives at, decode fills in the decoded
  line (mnemonic, operands, PC-relative target).

  Any word whose decoded instruction would NOT re-encode to the same bits is
  rejected by setting out->is_data = 1 and returning; the driver then renders it
  as `.FILL`. Each decoder is therefore responsible only for its own opcode's
  "is this actually a valid encoding of me?" check.
*/
typedef struct DisassemblyOperation {
  const Operation *operation;
  void (*decode)(uint16_t word, uint16_t address, DisassemblyInstruction *out);
} DisassemblyOperation;

/* Indexed by opcode; OP_RES has no decoding and is NULL (always data). */
extern const DisassemblyOperation *const DISASSEMBLY_OPERATIONS[OPERATION_COUNT];

const DisassemblyOperation *disassembly_operation_by_code(uint16_t code);

#endif /* DISASSEMBLER_OPERATION_H */
