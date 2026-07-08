#ifndef ASSEMBLER_DIRECTIVE_H
#define ASSEMBLER_DIRECTIVE_H

#include "core/assembly_directives.h"   /* Directive (the shared base) */
#include "assembler/assembler.h"        /* AssemblyInstruction */
#include "assembler/symbol_table.h"     /* SymbolTable */

/*
  The assembler's "subclass" of Directive: it points at the shared base (for its
  name) and adds the two behaviors that steer assembly — the mirror of how
  AssemblyOperation wraps Operation with `encode`.

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
typedef struct AssemblyDirective {
  const Directive *directive;
  int (*size)(const AssemblyInstruction *line);
  int (*encode)(AssemblyInstruction *line, const SymbolTable *table);
} AssemblyDirective;

/* Resolve a mnemonic (already upper-cased) to its AssemblyDirective, or NULL if
   the token is not a directive. For a bare identity check ("is this .ORIG?")
   the core directive_by_name is enough; use this only when the size/encode
   behavior is needed. */
const AssemblyDirective *assembly_directive_by_name(const char *upper_name);

#endif /* ASSEMBLER_DIRECTIVE_H */
