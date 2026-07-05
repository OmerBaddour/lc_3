#ifndef ASSEMBLER_DIRECTIVE_H
#define ASSEMBLER_DIRECTIVE_H

#include "assembler/assembler.h"      /* AssemblyInstruction */
#include "assembler/symbol_table.h"   /* SymbolTable */

/*
  An assembler directive (.ORIG, .END, .FILL, .BLKW, .STRINGZ). Unlike an
  Operation or a Trap, a directive has NO presence in the running machine — it
  only steers assembly — so there is a single struct here, not a core base plus
  per-context subclasses.

  Two behaviors, mirroring the two passes:
    - `size`   : how many program words this line occupies (drives the location
                 counter in pass 1, and the emit size in pass 2). Data-dependent
                 for .BLKW / .STRINGZ, so it is a function of the line.
    - `encode` : fill line->machine_codes with the emitted words. NULL for
                 directives that emit nothing (.ORIG, .END). Returns 1 on
                 success, 0 on error (e.g. .FILL of an undefined label). The
                 symbol table is passed to every encoder; those that do not need
                 it (void) it.
*/
typedef struct Directive {
  const char *name;   /* canonical spelling, upper-cased, incl. the dot: ".FILL" */
  int (*size)(const AssemblyInstruction *line);
  int (*encode)(AssemblyInstruction *line, const SymbolTable *table);
} Directive;

/* The single source of truth for each directive. Exposed (like OPERATION_* and
   TRAP_*) so callers that must special-case a specific directive compare pointers
   against these globals — `directive_by_name(x) == &DIRECTIVE_ORIG` — instead of
   re-spelling ".ORIG" in a strcmp. */
extern const Directive DIRECTIVE_ORIG;
extern const Directive DIRECTIVE_END;
extern const Directive DIRECTIVE_FILL;
extern const Directive DIRECTIVE_BLKW;
extern const Directive DIRECTIVE_STRINGZ;

/* Resolve a mnemonic (already upper-cased) to its Directive, or NULL if the
   token is not a directive (an opcode, a trap alias, or a plain label). */
const Directive *directive_by_name(const char *upper_name);

#endif /* ASSEMBLER_DIRECTIVE_H */
