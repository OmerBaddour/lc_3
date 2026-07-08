#include <string.h>
#include "core/assembly_directives.h"

/* One const global per directive — the single source of truth for its name.
   Mirrors the OPERATIONS pattern in core/operation.c and TRAPS in core/trap.c. */
const Directive DIRECTIVE_ORIG    = { .name = ".ORIG"    };
const Directive DIRECTIVE_END     = { .name = ".END"     };
const Directive DIRECTIVE_FILL    = { .name = ".FILL"    };
const Directive DIRECTIVE_BLKW    = { .name = ".BLKW"    };
const Directive DIRECTIVE_STRINGZ = { .name = ".STRINGZ" };

const Directive *const DIRECTIVES[DIRECTIVE_COUNT] = {
    &DIRECTIVE_ORIG, &DIRECTIVE_END, &DIRECTIVE_FILL,
    &DIRECTIVE_BLKW, &DIRECTIVE_STRINGZ,
};

const Directive *directive_by_name(const char *upper_name) {
  for (int i = 0; i < DIRECTIVE_COUNT; i++) {
    if (strcmp(DIRECTIVES[i]->name, upper_name) == 0) {
      return DIRECTIVES[i];
    }
  }
  return NULL;
}
