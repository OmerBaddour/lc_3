#include <string.h>
#include "core/trap.h"

/* One const global per trap — the single source of truth for {code, name}.
   Mirrors the OPERATIONS pattern in core/operation.c. */
const Trap TRAP_GETC  = { .code = 0x20, .name = "GETC"  };
const Trap TRAP_OUT   = { .code = 0x21, .name = "OUT"   };
const Trap TRAP_PUTS  = { .code = 0x22, .name = "PUTS"  };
const Trap TRAP_IN    = { .code = 0x23, .name = "IN"    };
const Trap TRAP_PUTSP = { .code = 0x24, .name = "PUTSP" };
const Trap TRAP_HALT  = { .code = 0x25, .name = "HALT"  };

const Trap *const TRAPS[TRAP_COUNT] = {
    &TRAP_GETC, &TRAP_OUT, &TRAP_PUTS,
    &TRAP_IN,   &TRAP_PUTSP, &TRAP_HALT,
};

/* Trap vectors are sparse (they start at 0x20), so we scan rather than index
   an array by code — unlike operation_by_code. */
const Trap *trap_by_code(uint16_t code) {
  for (uint16_t i = 0; i < TRAP_COUNT; i++) {
    if (TRAPS[i]->code == code) {
      return TRAPS[i];
    }
  }
  return NULL;
}

/* Exact, case-sensitive match against the canonical mnemonic. The assembler
   upper-cases the token before calling, since assembly is case-insensitive. */
const Trap *trap_by_name(const char *name) {
  for (uint16_t i = 0; i < TRAP_COUNT; i++) {
    if (strcmp(TRAPS[i]->name, name) == 0) {
      return TRAPS[i];
    }
  }
  return NULL;
}
