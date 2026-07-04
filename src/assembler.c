#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "assembler.h"
#include "util.h"       /* split(), trim(), free_string_list() — reused from the VM side */
#include "registers.h"  /* register_by_name() */

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
 * NOTE: this hard-coded table is a placeholder. Once the Operation table
 * exists it becomes `operation_by_name(...) != NULL` plus a directive
 * check — one source of truth instead of this list.
 * ------------------------------------------------------------------ */
static const char *OPERATION_NAMES[] = {
  "ADD", "AND", "NOT", "LD", "LDI", "LDR", "LEA",
  "ST", "STI", "STR", "JMP", "JSR", "JSRR", "RET", "RTI", "TRAP",
  /* trap aliases */
  "GETC", "OUT", "PUTS", "IN", "PUTSP", "HALT",
  /* assembler directives */
  ".ORIG", ".END", ".FILL", ".BLKW", ".STRINGZ",
};

static int is_operation_name(const char *token) {
  char *upper = to_upper_copy(token);
  int found = 0;

  for (size_t i = 0; i < sizeof OPERATION_NAMES / sizeof OPERATION_NAMES[0]; i++) {
    if (strcmp(upper, OPERATION_NAMES[i]) == 0) {
      found = 1;
      break;
    }
  }

  /* BR has condition-code suffixes: BR, BRn, BRz, BRp, BRnz, BRnzp, ... */
  if (!found && upper[0] == 'B' && upper[1] == 'R') {
    found = 1;
    for (size_t i = 2; upper[i] != '\0'; i++) {
      if (upper[i] != 'N' && upper[i] != 'Z' && upper[i] != 'P') {
        found = 0;
        break;
      }
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
 * PASS 2 support: symbol table, sizing, encoding, object-file dump
 *
 * All of this is deliberately self-contained in assembler.c so the
 * milestone stays runnable while operations.c is mid-refactor. The
 * long-term home for encoding is an `assemble` function pointer on the
 * Operation table (mirror of the VM's `execute`) — see project debt.
 * ================================================================== */

/* --- symbol table: label -> address, a growable array of records ---
   Linear scan on lookup, exactly like register_by_name — at assembly
   scale (tens/hundreds of labels) a hash table would be premature. */
typedef struct {
  char *label;        /* OWNED copy of the label text */
  uint16_t address;
} Symbol;

typedef struct {
  Symbol *data;
  int length;
  int capacity;
} SymbolTable;

static void symbol_table_insert(SymbolTable *table, const char *label, uint16_t address) {
  if (table->length == table->capacity) {
    table->capacity = table->capacity ? table->capacity * 2 : 8;
    table->data = realloc(table->data, (size_t) table->capacity * sizeof *table->data);
  }
  table->data[table->length].label = strdup(label);
  table->data[table->length].address = address;
  table->length++;
}

/* Returns 1 and writes *out on hit; 0 on miss. */
static int symbol_table_get(const SymbolTable *table, const char *label, uint16_t *out) {
  for (int i = 0; i < table->length; i++) {
    if (strcmp(table->data[i].label, label) == 0) {
      *out = table->data[i].address;
      return 1;
    }
  }
  return 0;
}

static void symbol_table_free(SymbolTable *table) {
  for (int i = 0; i < table->length; i++) {
    free(table->data[i].label);
  }
  free(table->data);
}

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

/* Resolve a label to a PC-relative offset, masked to `bits` and range-checked.
   PC-relative because the LC-3 adds the offset to the *incremented* PC, so the
   base is (instr_addr + 1). Sets *ok = 0 (and leaves it if already 0) on error. */
static uint16_t symbol_table_compute_pc_offset(const SymbolTable *table, const char *label,
                          uint16_t instr_addr, int bits, int *ok) {
  uint16_t target;
  if (!symbol_table_get(table, label, &target)) {
    fprintf(stderr, "error: undefined label '%s'\n", label);
    *ok = 0;
    return 0;
  }
  int offset = (int) target - (int) (instr_addr + 1);
  int min = -(1 << (bits - 1));
  int max =  (1 << (bits - 1)) - 1;
  if (offset < min || offset > max) {
    fprintf(stderr, "error: label '%s' unreachable (offset %d exceeds %d-bit field)\n",
            label, offset, bits);
    *ok = 0;
    return 0;
  }
  return (uint16_t) (offset & ((1 << bits) - 1));
}

/* Mask a signed immediate to `bits`, range-checking first. The mask alone IS
   two's-complement encoding (-5 & 0x1F == 0x1B, which sign_extend recovers);
   the check exists because out-of-range values would otherwise wrap silently
   (#16 in imm5 would read back as -16). Sets *ok = 0 on error. */
static uint16_t immediate_value_to_bits(int value, int bits, int *ok) {
  int min = -(1 << (bits - 1));
  int max =  (1 << (bits - 1)) - 1;
  if (value < min || value > max) {
    fprintf(stderr, "error: immediate %d does not fit in a signed %d-bit field [%d, %d]\n",
            value, bits, min, max);
    *ok = 0;
    return 0;
  }
  return (uint16_t) (value & ((1 << bits) - 1));
}

static uint16_t operand_register_get_code(const Operand *op) {
  assert(op->type == OPERAND_REGISTER);
  return op->as.reg->code;
}

/* Encode one ORDINARY instruction line (not a directive) into its 16-bit word.
   Operands are already classified; we trust well-formed input here and only
   error on unresolved/out-of-range labels — richer validation is future work. */
static uint16_t assembly_instruction_ordinary_to_bits(const AssemblyInstruction *line,
                                    const char *upper, const SymbolTable *table, int *ok) {
  const Operand *op = line->operands;

  /* ADD / AND share a shape: reg-reg or reg-imm5 selected by bit 5. */
  if (strcmp(upper, "ADD") == 0 || strcmp(upper, "AND") == 0) {
    uint16_t opc = (upper[0] == 'A' && upper[1] == 'D') ? 0x1 : 0x5;
    uint16_t word = (uint16_t) ((opc << 12) | (operand_register_get_code(&op[0]) << 9) | (operand_register_get_code(&op[1]) << 6));
    if (op[2].type == OPERAND_REGISTER) {
      word |= operand_register_get_code(&op[2]);                     /* bit 5 = 0, SR2 in bits 2-0 */
    } else {
      word |= (uint16_t) ((1 << 5) | immediate_value_to_bits(op[2].as.integer, 5, ok));  /* bit 5 = 1, imm5 */
    }
    return word;
  }

  if (strcmp(upper, "NOT") == 0)
    return (uint16_t) ((0x9 << 12) | (operand_register_get_code(&op[0]) << 9) | (operand_register_get_code(&op[1]) << 6) | 0x3F);

  /* BR[nzp]: the condition bits live in the mnemonic suffix. Bare BR = BRnzp. */
  if (upper[0] == 'B' && upper[1] == 'R') {
    uint16_t nzp = 0;
    const char *cc = upper + 2;
    if (*cc == '\0') {
      nzp = 0x7;
    } else {
      for (; *cc; cc++) {
        if      (*cc == 'N') nzp |= 0x4;
        else if (*cc == 'Z') nzp |= 0x2;
        else if (*cc == 'P') nzp |= 0x1;
      }
    }
    return (uint16_t) ((0x0 << 12) | (nzp << 9) | symbol_table_compute_pc_offset(table, op[0].as.label, line->address, 9, ok));
  }

  if (strcmp(upper, "JMP") == 0) return (uint16_t) ((0xC << 12) | (operand_register_get_code(&op[0]) << 6));
  if (strcmp(upper, "RET") == 0) return (uint16_t) ((0xC << 12) | (0x7 << 6));  /* JMP R7 */

  if (strcmp(upper, "JSR")  == 0)
    return (uint16_t) ((0x4 << 12) | (1 << 11) | symbol_table_compute_pc_offset(table, op[0].as.label, line->address, 11, ok));
  if (strcmp(upper, "JSRR") == 0)
    return (uint16_t) ((0x4 << 12) | (operand_register_get_code(&op[0]) << 6));

  /* PC-relative load/stores: opcode | DR/SR | PCoffset9. */
  struct { const char *name; uint16_t opc; } pcrel[] = {
    { "LD", 0x2 }, { "LDI", 0xA }, { "LEA", 0xE }, { "ST", 0x3 }, { "STI", 0xB },
  };
  for (size_t i = 0; i < sizeof pcrel / sizeof pcrel[0]; i++) {
    if (strcmp(upper, pcrel[i].name) == 0)
      return (uint16_t) ((pcrel[i].opc << 12) | (operand_register_get_code(&op[0]) << 9)
                         | symbol_table_compute_pc_offset(table, op[1].as.label, line->address, 9, ok));
  }

  /* base+offset load/stores: opcode | DR/SR | BaseR | offset6. */
  if (strcmp(upper, "LDR") == 0 || strcmp(upper, "STR") == 0) {
    uint16_t opc = (upper[0] == 'L') ? 0x6 : 0x7;
    return (uint16_t) ((opc << 12) | (operand_register_get_code(&op[0]) << 9) | (operand_register_get_code(&op[1]) << 6)
                       | immediate_value_to_bits(op[2].as.integer, 6, ok));
  }

  if (strcmp(upper, "TRAP") == 0) {
    /* trapvect8 is UNSIGNED (a vector-table index), unlike imm5/offset6. */
    int vector = op[0].as.integer;
    if (vector < 0 || vector > 0xFF) {
      fprintf(stderr, "error: trap vector %d does not fit in 8 bits [0, 255]\n", vector);
      *ok = 0;
      return 0;
    }
    return (uint16_t) ((0xF << 12) | vector);
  }
  if (strcmp(upper, "RTI")  == 0) return (uint16_t) (0x8 << 12);

  uint16_t vector;
  if (trap_vector(upper, &vector)) return (uint16_t) ((0xF << 12) | vector);

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

