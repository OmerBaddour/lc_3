#ifndef CORE_ASSEMBLY_DIRECTIVES_H
#define CORE_ASSEMBLY_DIRECTIVES_H

#define DIRECTIVE_COUNT 5

/*
  The SHARED identity of an assembler directive (.ORIG, .END, .FILL, .BLKW,
  .STRINGZ): its canonical spelling, and nothing else. This is the "base class",
  mirroring core/operation.c's Operation and core/trap.c's Trap.

  Unlike an operation or a trap, a directive has NO presence in the running
  machine — the VM never sees one; it is purely a text-processing concept. So it
  lives here, in the core shared by the two text-facing tools (the ASSEMBLER,
  which adds size/encode as AssemblyDirective; and the DISASSEMBLER, which needs
  only the name, e.g. ".FILL", and so uses this base directly), and is kept out
  of the operation/trap files that the VM also links against.
*/
typedef struct Directive {
  const char *name;   /* canonical spelling, upper-cased, incl. the dot: ".FILL" */
} Directive;

/* The single source of truth for each directive. Exposed (like OPERATION_* and
   TRAP_*) so callers that must special-case a specific directive compare
   pointers against these globals — `directive_by_name(x) == &DIRECTIVE_ORIG` —
   instead of re-spelling ".ORIG" in a strcmp. */
extern const Directive DIRECTIVE_ORIG;
extern const Directive DIRECTIVE_END;
extern const Directive DIRECTIVE_FILL;
extern const Directive DIRECTIVE_BLKW;
extern const Directive DIRECTIVE_STRINGZ;

extern const Directive *const DIRECTIVES[DIRECTIVE_COUNT];

/* Resolve a mnemonic (already upper-cased) to its Directive, or NULL if the
   token is not a directive (an opcode, a trap alias, or a plain label). */
const Directive *directive_by_name(const char *upper_name);

#endif /* CORE_ASSEMBLY_DIRECTIVES_H */
