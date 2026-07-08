#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assembler/directive.h"

/* ------------------------------------------------------------------ *
 * sizing: how many program words does this directive occupy?
 * ------------------------------------------------------------------ */

/* .ORIG / .END steer assembly but emit nothing. */
static int directive_zero_size(const AssemblyInstruction *line) {
  (void)line;
  return 0;
}

/* .FILL is exactly one word. */
static int directive_fill_size(const AssemblyInstruction *line) {
  (void)line;
  return 1;
}

/* .BLKW reserves a run of `n` words (n given by its operand). */
static int directive_blkw_size(const AssemblyInstruction *line) {
  return line->operands_length ? line->operands[0].as.integer : 0;
}

/* .STRINGZ is one word per character plus one for the trailing NUL word. */
static int directive_stringz_size(const AssemblyInstruction *line) {
  return line->operands_length ? (int) strlen(line->operands[0].as.string) + 1 : 0;
}

/* ------------------------------------------------------------------ *
 * encoding: fill line->machine_codes with the emitted words
 * ------------------------------------------------------------------ */

/* .FILL holds a raw word: either a literal, or a label's ABSOLUTE address
   (not PC-relative — the value stored *is* the address). */
static int directive_fill_encode(AssemblyInstruction *line, const SymbolTable *table) {
  uint16_t value = 0;
  int ok = 1;
  if (line->operands[0].type == OPERAND_LABEL) {
    if (!symbol_table_get(table, line->operands[0].as.label, &value)) {
      fprintf(stderr, "error: undefined label '%s'\n", line->operands[0].as.label);
      ok = 0;
    }
  } else {
    value = (uint16_t) line->operands[0].as.integer;
  }
  line->machine_codes = malloc(sizeof(uint16_t));
  line->machine_codes[0] = value;
  line->machine_codes_length = 1;
  return ok;
}

/* .BLKW reserves n zeroed words. */
static int directive_blkw_encode(AssemblyInstruction *line, const SymbolTable *table) {
  (void)table;
  int n = line->operands_length ? line->operands[0].as.integer : 0;
  line->machine_codes = calloc((size_t) (n > 0 ? n : 1), sizeof(uint16_t));
  line->machine_codes_length = n;
  return 1;
}

/* .STRINGZ emits one word per character, NUL-terminated. */
static int directive_stringz_encode(AssemblyInstruction *line, const SymbolTable *table) {
  (void)table;
  const char *s = line->operands[0].as.string;
  int n = (int) strlen(s) + 1;                        /* +1 for the trailing NUL word */
  line->machine_codes = malloc((size_t) n * sizeof(uint16_t));
  for (int i = 0; i < n; i++) {
    line->machine_codes[i] = (uint16_t) (unsigned char) s[i];  /* s[len] == '\0' -> 0 */
  }
  line->machine_codes_length = n;
  return 1;
}

/* ------------------------------------------------------------------ *
 * the assembler subclass table + lookup: each entry pairs a shared Directive
 * base (its name) with this side's size/encode behavior. Indexed the same way
 * ASSEMBLY_OPERATIONS is — by pairing, not by opcode, since directives have no
 * numeric code.
 * ------------------------------------------------------------------ */
static const AssemblyDirective ASSEMBLY_DIRECTIVE_ORIG    = { &DIRECTIVE_ORIG,    directive_zero_size,    NULL };
static const AssemblyDirective ASSEMBLY_DIRECTIVE_END     = { &DIRECTIVE_END,     directive_zero_size,    NULL };
static const AssemblyDirective ASSEMBLY_DIRECTIVE_FILL    = { &DIRECTIVE_FILL,    directive_fill_size,    directive_fill_encode };
static const AssemblyDirective ASSEMBLY_DIRECTIVE_BLKW    = { &DIRECTIVE_BLKW,    directive_blkw_size,    directive_blkw_encode };
static const AssemblyDirective ASSEMBLY_DIRECTIVE_STRINGZ = { &DIRECTIVE_STRINGZ, directive_stringz_size, directive_stringz_encode };

static const AssemblyDirective *const ASSEMBLY_DIRECTIVES[DIRECTIVE_COUNT] = {
    &ASSEMBLY_DIRECTIVE_ORIG, &ASSEMBLY_DIRECTIVE_END, &ASSEMBLY_DIRECTIVE_FILL,
    &ASSEMBLY_DIRECTIVE_BLKW, &ASSEMBLY_DIRECTIVE_STRINGZ,
};

const AssemblyDirective *assembly_directive_by_name(const char *upper_name) {
  for (int i = 0; i < DIRECTIVE_COUNT; i++) {
    if (strcmp(ASSEMBLY_DIRECTIVES[i]->directive->name, upper_name) == 0) {
      return ASSEMBLY_DIRECTIVES[i];
    }
  }
  return NULL;
}
