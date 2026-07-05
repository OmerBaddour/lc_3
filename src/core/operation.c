#include <string.h>
#include "core/operation.h"

/* One const global per opcode — the single source of truth for {code, name}.
   Mirrors the REGISTERS pattern in core/registers.c. */
const Operation OPERATION_BR   = { .code = OP_BR,   .name = "BR"   };
const Operation OPERATION_ADD  = { .code = OP_ADD,  .name = "ADD"  };
const Operation OPERATION_LD   = { .code = OP_LD,   .name = "LD"   };
const Operation OPERATION_ST   = { .code = OP_ST,   .name = "ST"   };
const Operation OPERATION_JSR  = { .code = OP_JSR,  .name = "JSR"  };
const Operation OPERATION_AND  = { .code = OP_AND,  .name = "AND"  };
const Operation OPERATION_LDR  = { .code = OP_LDR,  .name = "LDR"  };
const Operation OPERATION_STR  = { .code = OP_STR,  .name = "STR"  };
const Operation OPERATION_RTI  = { .code = OP_RTI,  .name = "RTI"  };
const Operation OPERATION_NOT  = { .code = OP_NOT,  .name = "NOT"  };
const Operation OPERATION_LDI  = { .code = OP_LDI,  .name = "LDI"  };
const Operation OPERATION_STI  = { .code = OP_STI,  .name = "STI"  };
const Operation OPERATION_JMP  = { .code = OP_JMP,  .name = "JMP"  };
const Operation OPERATION_RES  = { .code = OP_RES,  .name = "RES"  };
const Operation OPERATION_LEA  = { .code = OP_LEA,  .name = "LEA"  };
const Operation OPERATION_TRAP = { .code = OP_TRAP, .name = "TRAP" };

const Operation *const OPERATIONS[OPERATION_COUNT] = {
    &OPERATION_BR,  &OPERATION_ADD, &OPERATION_LD,  &OPERATION_ST,
    &OPERATION_JSR, &OPERATION_AND, &OPERATION_LDR, &OPERATION_STR,
    &OPERATION_RTI, &OPERATION_NOT, &OPERATION_LDI, &OPERATION_STI,
    &OPERATION_JMP, &OPERATION_RES, &OPERATION_LEA, &OPERATION_TRAP,
};

const Operation *operation_by_code(uint16_t code) {
  if (code >= OPERATION_COUNT) {
    return NULL;
  }
  return OPERATIONS[code];
}

/* Exact, case-sensitive match against the canonical mnemonic. Mnemonic families
   (BR condition-code suffixes, RET, JSRR) are an assembler-parsing concern, and
   live in assembler/operation.c's assembly_operation_by_name — not here. */
const Operation *operation_by_name(const char *name) {
  for (uint16_t i = 0; i < OPERATION_COUNT; i++) {
    if (strcmp(OPERATIONS[i]->name, name) == 0) {
      return OPERATIONS[i];
    }
  }
  return NULL;
}
