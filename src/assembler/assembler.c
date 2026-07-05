#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "assembler/assembler.h"
#include "assembler/operation.h"     /* assembly_operation_by_name(), the encode table */
#include "assembler/symbol_table.h"  /* SymbolTable + symbol_table_* */
#include "core/util.h"       /* split(), trim(), free_string_list() — reused from the VM side */
#include "core/registers.h"  /* register_by_name() */

/* ------------------------------------------------------------------ *
 * small string helpers
 * ------------------------------------------------------------------ */

/* Return a heap copy of `s` with every letter upper-cased. Caller frees.
   Assembly is case-insensitive ("PUTs" == "PUTS"), so we compare in upper. */
static char *to_upper_copy(const char *s) {
  size_t n = strlen(s);
  char *out = malloc(n + 1);
  for (size_t i = 0; i < n; i++) {
    out[i] = (char) toupper((unsigned char) s[i]);
  }
  out[n] = '\0';
  return out;
}

/* ------------------------------------------------------------------ *
 * is this token an operation name (an opcode / directive / trap alias)?
 *
 * We need this to answer the first parsing question on every line:
 * "is the first word a LABEL, or the OPERATION NAME itself?" A line may
 * start with an optional label, so the only way to tell `LOOP` from `ADD`
 * is to know the set of real operation names.
 *
 * Opcodes (incl. the BR/RET/JSRR families) now come from the Operation table
 * via assembly_operation_by_name. Trap aliases and directives are still listed
 * here until their own tables land (M2/M3).
 * ------------------------------------------------------------------ */
static const char *NON_OPERATION_NAMES[] = {
  /* trap aliases */
  "GETC", "OUT", "PUTS", "IN", "PUTSP", "HALT",
  /* assembler directives */
  ".ORIG", ".END", ".FILL", ".BLKW", ".STRINGZ",
};

static int is_operation_name(const char *token) {
  char *upper = to_upper_copy(token);
  int found = (assembly_operation_by_name(upper) != NULL);

  for (size_t i = 0; !found && i < sizeof NON_OPERATION_NAMES / sizeof NON_OPERATION_NAMES[0]; i++) {
    if (strcmp(upper, NON_OPERATION_NAMES[i]) == 0) {
      found = 1;
    }
  }

  free(upper);
  return found;
}

/* ------------------------------------------------------------------ *
 * classify a single operand token into a tagged-union Operand
 *
 * Returns the Operand BY VALUE. The struct is tiny (a tag + a one-pointer
 * union) and is built from scratch, so returning a copy is cleaner than the
 * C out-parameter idiom (`void f(..., Operand *out)`) — that idiom earns its
 * keep only for large structs, or when the return slot is needed for an error
 * code. NOTE the copy is SHALLOW: for OPERAND_LABEL the `char *` pointer is
 * duplicated but the heap it points at is not, so ownership simply moves to
 * the caller (who stores it). Free it in exactly one place.
 * ------------------------------------------------------------------ */
static Operand build_operand(const char *token) {
  Operand operand;

  /* (1) a register? register_by_name is exact/case-sensitive, so upper-case
         first to accept r0 as well as R0. */
  char *upper = to_upper_copy(token);
  const Register *reg = register_by_name(upper);
  free(upper);
  if (reg != NULL) {
    operand.type = OPERAND_REGISTER;
    operand.as.reg = reg;
    return operand;
  }

  /* (2) an integer immediate? LC-3 spellings:
         #5 / #-5  decimal,  x1F  hex,  0x1F hex,  42 bare decimal. */
  int base = 0;
  const char *digits = NULL;
  if (token[0] == '#') {
    base = 10; digits = token + 1;
  } else if (token[0] == 'x' || token[0] == 'X') {
    base = 16; digits = token + 1;
  } else if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
    base = 16; digits = token + 2;
  } else if (token[0] == '-' || isdigit((unsigned char) token[0])) {
    base = 10; digits = token;
  }

  if (digits != NULL) {
    char *end = NULL;
    long value = strtol(digits, &end, base);
    /* strtol sets `end` to the first char it could NOT parse. If that is the
       terminating '\0' (and we consumed at least one digit), the whole token
       was a valid number. */
    if (end != digits && *end == '\0') {
      operand.type = OPERAND_INTEGER;
      operand.as.integer = (int) value;
      return operand;
    }
  }

  /* (3) otherwise it is a label reference (e.g. the LOOP in `BR LOOP`). */
  operand.type = OPERAND_LABEL;
  operand.as.label = strdup(token);
  return operand;
}

/* ------------------------------------------------------------------ *
 * parse one line
 * ------------------------------------------------------------------ */
AssemblyInstruction *parse_assembly_line(const char *line) {
  /* Keep the original for error messages before we chop it up. */
  char *raw = strdup(line);

  /* (a) strip the comment: everything from ';' to end of line. */
  char *no_comment = strdup(line);
  char *semicolon = strchr(no_comment, ';');
  if (semicolon != NULL) {
    *semicolon = '\0';
  }

  /* (b) trim surrounding whitespace. If nothing is left, this line carried
         only a comment or blank space — not an instruction. */
  char *trimmed = trim(no_comment);
  free(no_comment);
  if (trimmed[0] == '\0') {
    free(raw);
    free(trimmed);
    return NULL;
  }

  AssemblyInstruction *instruction = calloc(1, sizeof *instruction);
  instruction->raw = raw;

  /* (c) peel the first two whitespace-separated words. strtok_r hands back a
         token and leaves `save` pointing at the untouched remainder — which
         is exactly the operand region once we've taken the operation name. */
  char *save = NULL;
  char *first = strtok_r(trimmed, " \t", &save);

  const char *operation_name = NULL;
  char *operand_region = NULL;

  if (is_operation_name(first)) {
    /* No label on this line: `ADD R0, R1, R2` */
    operation_name = first;
    operand_region = save;
  } else {
    /* First word is a label: `LOOP  ADD R0, R1, R2` or `HELLO .STRINGZ "..."` */
    instruction->label = strdup(first);
    char *second = strtok_r(NULL, " \t", &save);
    operation_name = second;        /* may be NULL: a label sitting on its own line */
    operand_region = save;
  }

  if (operation_name != NULL) {
    instruction->operation_name = strdup(operation_name);
  }

  /* (d) parse the operands. */
  char *upper_operation_name = operation_name ? to_upper_copy(operation_name) : NULL;

  if (upper_operation_name && strcmp(upper_operation_name, ".STRINGZ") == 0) {
    /* Directives with string literals can't be tokenized like registers —
       the text between the quotes may contain spaces (and commas). Grab the
       whole quoted span as a single operand. Real directive handling will
       live in assembly_directives.c; this is a deliberate stopgap. */
    char *open = operand_region ? strchr(operand_region, '"') : NULL;
    char *close = open ? strrchr(open + 1, '"') : NULL;
    if (open && close && close > open) {
      size_t len = (size_t)(close - open - 1);
      char *text = malloc(len + 1);
      memcpy(text, open + 1, len);
      text[len] = '\0';
      instruction->operands[0].type = OPERAND_STRING;
      instruction->operands[0].as.string = text;
      instruction->operands_length = 1;
    }
  } else if (operand_region != NULL) {
    char *op_trimmed = trim(operand_region);
    if (op_trimmed[0] != '\0') {
      /* Operands are comma-separated; reuse the VM's split(), which also
         trims each piece: "R0, R1, R2" -> ["R0", "R1", "R2"]. */
      StringList *pieces = split(op_trimmed, ',');
      for (uint16_t i = 0; i < pieces->length && instruction->operands_length < MAX_OPERANDS; i++) {
        instruction->operands[instruction->operands_length] = build_operand(pieces->data[i]);
        instruction->operands_length++;
      }
      free_string_list(pieces);
    }
    free(op_trimmed);
  }

  free(upper_operation_name);
  free(trimmed);
  return instruction;
}

/* ------------------------------------------------------------------ *
 * cleanup + debug printing
 * ------------------------------------------------------------------ */
void free_assembly_instruction(AssemblyInstruction *instruction) {
  if (instruction == NULL) {
    return;
  }
  /* OPERAND_LABEL and OPERAND_STRING each own a heap string; registers point
     into the shared table and integers own nothing — so we free selectively
     by tag. (label and string alias the same union storage, but reading via
     the member the tag names keeps intent clear.) */
  for (int i = 0; i < instruction->operands_length; i++) {
    switch (instruction->operands[i].type) {
      case OPERAND_LABEL:  free(instruction->operands[i].as.label);  break;
      case OPERAND_STRING: free(instruction->operands[i].as.string); break;
      case OPERAND_REGISTER:
      case OPERAND_INTEGER:
        break;  /* nothing owned */
    }
  }
  free(instruction->raw);
  free(instruction->label);
  free(instruction->operation_name);
  free(instruction->machine_codes);
  free(instruction);
}

void print_assembly_instruction(const AssemblyInstruction *instruction) {
  printf("label=%-10s operation_name=%-10s operands=[",
         instruction->label ? instruction->label : "-",
         instruction->operation_name ? instruction->operation_name : "-");

  for (int i = 0; i < instruction->operands_length; i++) {
    const Operand *op = &instruction->operands[i];
    if (i > 0) {
      printf(", ");
    }
    switch (op->type) {
      case OPERAND_REGISTER: printf("reg:%s", op->as.reg->name);   break;
      case OPERAND_INTEGER:  printf("int:%d", op->as.integer);     break;
      case OPERAND_LABEL:    printf("label:%s", op->as.label);     break;
      case OPERAND_STRING:   printf("str:%s", op->as.string);      break;
    }
  }
  printf("]\n");
}

/* ================================================================== *
 * PASS 2 support: sizing, encoding, object-file dump
 *
 * The symbol table lives in assembler/symbol_table.c and the per-operation
 * encoders in assembler/operation.c (the mirror of the VM's `execute`). What
 * remains here is the two-pass driver, directive sizing/encoding (until the
 * directive table lands in M3), and the trap-alias fallback (until M2).
 * ================================================================== */

/* --- trap aliases: mnemonic -> trap vector (the vector goes in bits 7-0
   of a TRAP instruction). Returns 1 on match. --- */
static int trap_vector(const char *upper, uint16_t *vector) {
  if      (strcmp(upper, "GETC")  == 0) { *vector = 0x20; }
  else if (strcmp(upper, "OUT")   == 0) { *vector = 0x21; }
  else if (strcmp(upper, "PUTS")  == 0) { *vector = 0x22; }
  else if (strcmp(upper, "IN")    == 0) { *vector = 0x23; }
  else if (strcmp(upper, "PUTSP") == 0) { *vector = 0x24; }
  else if (strcmp(upper, "HALT")  == 0) { *vector = 0x25; }
  else return 0;
  return 1;
}

/* --- how many program words does this instruction occupy? (drives the location
   counter in pass 1 and the emit size in pass 2). --- */
static int assembly_instruction_machine_code_length(const AssemblyInstruction *line, const char *upper) {
  if (upper == NULL) return 0;                                  /* label-only line */
  if (strcmp(upper, ".ORIG") == 0 || strcmp(upper, ".END") == 0) return 0;
  if (strcmp(upper, ".FILL")  == 0) return 1;
  if (strcmp(upper, ".BLKW")  == 0)
    return line->operands_length ? line->operands[0].as.integer : 0;
  if (strcmp(upper, ".STRINGZ") == 0)
    return line->operands_length ? (int) strlen(line->operands[0].as.string) + 1 : 0;
  return 1;                                                     /* ordinary instruction */
}

/* Encode one ORDINARY instruction line (not a directive) into its 16-bit word.
   Opcodes resolve through the assembler operation table's per-op `encode`; the
   only case that is not (yet) an Operation is a trap alias (GETC, OUT, ...),
   which encodes to a TRAP word — handled inline until the trap table lands. */
static uint16_t assembly_instruction_ordinary_to_bits(const AssemblyInstruction *line,
                                    const char *upper, const SymbolTable *table, int *ok) {
  const AssemblyOperation *assembly_operation = assembly_operation_by_name(upper);
  if (assembly_operation != NULL) {
    return assembly_operation->encode(line, table, ok);
  }

  uint16_t vector;
  if (trap_vector(upper, &vector)) {
    return (uint16_t) ((0xF << 12) | vector);
  }

  fprintf(stderr, "error: unknown operation name '%s'\n", line->operation_name);
  *ok = 0;
  return 0;
}

/* Fill line->machine_codes for one line (directive data or one instruction word). */
static int assembly_instruction_compute_machine_codes(AssemblyInstruction *line, const char *upper, const SymbolTable *table) {
  line->machine_codes = NULL;
  line->machine_codes_length = 0;

  if (upper == NULL || strcmp(upper, ".ORIG") == 0 || strcmp(upper, ".END") == 0)
    return 1;  /* occupies no program words */

  if (strcmp(upper, ".FILL") == 0) {
    /* .FILL holds a raw word: either a literal, or a label's ABSOLUTE address
       (not PC-relative — the value stored *is* the address). */
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

  if (strcmp(upper, ".BLKW") == 0) {
    int n = line->operands_length ? line->operands[0].as.integer : 0;
    line->machine_codes = calloc((size_t) (n > 0 ? n : 1), sizeof(uint16_t));
    line->machine_codes_length = n;
    return 1;
  }

  if (strcmp(upper, ".STRINGZ") == 0) {
    const char *s = line->operands[0].as.string;
    int n = (int) strlen(s) + 1;                        /* +1 for the trailing NUL word */
    line->machine_codes = malloc((size_t) n * sizeof(uint16_t));
    for (int i = 0; i < n; i++) {
      line->machine_codes[i] = (uint16_t) (unsigned char) s[i];  /* s[len] == '\0' -> 0 */
    }
    line->machine_codes_length = n;
    return 1;
  }

  int ok = 1;
  uint16_t word = assembly_instruction_ordinary_to_bits(line, upper, table, &ok);
  line->machine_codes = malloc(sizeof(uint16_t));
  line->machine_codes[0] = word;
  line->machine_codes_length = 1;
  return ok;
}

/* Two passes over the whole program. Writes the origin to *origin_out.
   Returns 1 on success, 0 if any line failed to assemble. */
int assemble(AssemblyInstruction **program, int count, uint16_t *origin_out) {
  SymbolTable table = {0};
  uint16_t origin = 0, lc = 0;
  int have_origin = 0;

  /* PASS 1: assign each line an address and record label definitions. */
  for (int i = 0; i < count; i++) {
    AssemblyInstruction *line = program[i];
    char *upper = line->operation_name ? to_upper_copy(line->operation_name) : NULL;

    if (upper && strcmp(upper, ".ORIG") == 0) {
      origin = (uint16_t) line->operands[0].as.integer;
      lc = origin;
      have_origin = 1;
      line->address = origin;
      free(upper);
      continue;
    }

    line->address = lc;
    if (line->label) {
      symbol_table_insert(&table, line->label, lc);
    }
    lc = (uint16_t) (lc + assembly_instruction_machine_code_length(line, upper));
    free(upper);
  }

  if (!have_origin) {
    fprintf(stderr, "error: program has no .ORIG directive\n");
    symbol_table_free(&table);
    return 0;
  }

  /* PASS 2: resolve + emit. */
  int ok = 1;
  for (int i = 0; i < count; i++) {
    AssemblyInstruction *line = program[i];
    char *upper = line->operation_name ? to_upper_copy(line->operation_name) : NULL;
    if (!assembly_instruction_compute_machine_codes(line, upper, &table)) {
      ok = 0;
    }
    free(upper);
  }

  symbol_table_free(&table);
  *origin_out = origin;
  return ok;
}

/* Write one 16-bit word big-endian (high byte first). Endian-clean: it does
   not depend on the host's byte order, and mirrors the loader's swap16. */
static void machine_code_to_bits(FILE *out, uint16_t word) {
  fputc((word >> 8) & 0xFF, out);
  fputc(word & 0xFF, out);
}

int write_object_file(const char *path, uint16_t origin,
                      AssemblyInstruction **program, int count) {
  FILE *out = fopen(path, "wb");
  if (!out) {
    fprintf(stderr, "could not open %s for writing\n", path);
    return 0;
  }
  machine_code_to_bits(out, origin);              /* header: the load address */
  for (int i = 0; i < count; i++) {
    for (int j = 0; j < program[i]->machine_codes_length; j++) {
      machine_code_to_bits(out, program[i]->machine_codes[j]);
    }
  }
  fclose(out);
  return 1;
}

