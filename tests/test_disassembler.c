#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disassembler/disassembler.h"
#include "disassembler/operation.h"
#include "assembler/assembler.h"   /* to re-assemble a disassembly and check it round-trips */

/* ------------------------------------------------------------------ *
 * helpers
 * ------------------------------------------------------------------ */

/* Decode a single word at `address` in isolation (no labeling pass). Mirrors
   what disassemble() does per word, but returns the bare decoded line so the
   unit tests can inspect mnemonic / operands / target / is_data directly. */
static DisassemblyInstruction decode_one(uint16_t word, uint16_t address) {
  DisassemblyInstruction instruction;
  memset(&instruction, 0, sizeof instruction);
  instruction.address = address;
  instruction.word = word;
  const DisassemblyOperation *disassembly_operation = disassembly_operation_by_code(word >> 12);
  if (disassembly_operation != NULL && disassembly_operation->decode != NULL) {
    disassembly_operation->decode(word, address, &instruction);
  } else {
    instruction.is_data = 1;
  }
  return instruction;
}

/* Assemble a stream of assembly text and flatten every line's machine_codes
   into one word array (excluding the origin). The shared workhorse: used both
   to assemble a real .asm file and to re-assemble a disassembly held in an
   in-memory buffer for the round trip. Does not close `input`. */
static uint16_t *assemble_stream_to_words(FILE *input, int *count_out, uint16_t *origin_out) {
  AssemblyInstruction **program = NULL;
  int count = 0, capacity = 0;
  char line[256];
  while (fgets(line, sizeof line, input)) {
    AssemblyInstruction *instruction = parse_assembly_line(line);
    if (instruction == NULL) {
      continue;
    }
    if (count == capacity) {
      capacity = capacity ? capacity * 2 : 16;
      program = realloc(program, (size_t) capacity * sizeof *program);
    }
    program[count++] = instruction;
  }

  uint16_t origin = 0;
  assert(assemble(program, count, &origin) == 1);

  /* flatten machine_codes across all lines */
  uint16_t *words = NULL;
  int words_length = 0, words_capacity = 0;
  for (int i = 0; i < count; i++) {
    for (int j = 0; j < program[i]->machine_codes_length; j++) {
      if (words_length == words_capacity) {
        words_capacity = words_capacity ? words_capacity * 2 : 32;
        words = realloc(words, (size_t) words_capacity * sizeof *words);
      }
      words[words_length++] = program[i]->machine_codes[j];
    }
  }

  for (int i = 0; i < count; i++) {
    free_assembly_instruction(program[i]);
  }
  free(program);

  *count_out = words_length;
  *origin_out = origin;
  return words;
}

static uint16_t *assemble_file_to_words(const char *path, int *count_out, uint16_t *origin_out) {
  FILE *input = fopen(path, "r");
  assert(input != NULL);   /* run from the repo root, as `make test` does */
  uint16_t *words = assemble_stream_to_words(input, count_out, origin_out);
  fclose(input);
  return words;
}

/* The core property: disassembling a program and re-assembling that text must
   reproduce the ORIGINAL machine code, word for word. The disassembly is
   rendered into an in-memory stream (open_memstream) and re-assembled straight
   from it — no disk I/O, so this stays fast enough to run over every word. */
static void assert_round_trips(const uint16_t *words, int count, uint16_t origin) {
  DisassemblyInstruction *program = disassemble(words, count, origin);

  char *buffer = NULL;
  size_t buffer_size = 0;
  FILE *stream = open_memstream(&buffer, &buffer_size);
  assert(stream != NULL);
  disassembly_render(stream, origin, program, count);
  fclose(stream);   /* flushes and NUL-terminates `buffer` */
  free(program);

  FILE *input = fmemopen(buffer, buffer_size, "r");
  assert(input != NULL);
  int count2 = 0;
  uint16_t origin2 = 0;
  uint16_t *words2 = assemble_stream_to_words(input, &count2, &origin2);
  fclose(input);
  free(buffer);

  assert(origin2 == origin);
  assert(count2 == count);
  assert(memcmp(words, words2, (size_t) count * sizeof *words) == 0);
  free(words2);
}

/* ------------------------------------------------------------------ *
 * decode: register-form and immediate-form instructions
 * ------------------------------------------------------------------ */
static void test_decode_simple_instructions(void) {
  DisassemblyInstruction d;

  /* ADD / AND, register form and imm5 form (inverse of the assembler cases) */
  d = decode_one((0x1 << 12) | (0 << 9) | (1 << 6) | 2, 0x3000);
  assert(strcmp(d.operation_name, "ADD") == 0 && strcmp(d.operands, "R0, R1, R2") == 0 && !d.has_target);
  d = decode_one((0x5 << 12) | (1 << 9) | (2 << 6) | 3, 0x3000);
  assert(strcmp(d.operation_name, "AND") == 0 && strcmp(d.operands, "R1, R2, R3") == 0);
  d = decode_one(0x103B, 0x3000);   /* ADD R0, R0, #-5 */
  assert(strcmp(d.operation_name, "ADD") == 0 && strcmp(d.operands, "R0, R0, #-5") == 0);
  d = decode_one((0x5 << 12) | (1 << 5), 0x3000);   /* AND R0, R0, #0 */
  assert(strcmp(d.operands, "R0, R0, #0") == 0);

  d = decode_one((0x9 << 12) | (3 << 9) | (4 << 6) | 0x3F, 0x3000);   /* NOT R3, R4 */
  assert(strcmp(d.operation_name, "NOT") == 0 && strcmp(d.operands, "R3, R4") == 0);

  d = decode_one((0xC << 12) | (2 << 6), 0x3000);   /* JMP R2 */
  assert(strcmp(d.operation_name, "JMP") == 0 && strcmp(d.operands, "R2") == 0);
  d = decode_one((0xC << 12) | (7 << 6), 0x3000);   /* RET */
  assert(strcmp(d.operation_name, "RET") == 0 && d.operands[0] == '\0');
  d = decode_one((0x4 << 12) | (6 << 6), 0x3000);   /* JSRR R6 */
  assert(strcmp(d.operation_name, "JSRR") == 0 && strcmp(d.operands, "R6") == 0);

  d = decode_one((0x6 << 12) | (1 << 9) | (2 << 6) | 0x3F, 0x3000);   /* LDR R1, R2, #-1 */
  assert(strcmp(d.operation_name, "LDR") == 0 && strcmp(d.operands, "R1, R2, #-1") == 0);
  d = decode_one((0x7 << 12) | (3 << 9) | (4 << 6) | 4, 0x3000);      /* STR R3, R4, #4 */
  assert(strcmp(d.operation_name, "STR") == 0 && strcmp(d.operands, "R3, R4, #4") == 0);

  d = decode_one(0x8000, 0x3000);   /* RTI */
  assert(strcmp(d.operation_name, "RTI") == 0 && d.operands[0] == '\0');

  /* traps: known vectors become aliases, unknown ones become `TRAP xNN` */
  assert(strcmp(decode_one(0xF020, 0x3000).operation_name, "GETC")  == 0);
  assert(strcmp(decode_one(0xF022, 0x3000).operation_name, "PUTS")  == 0);
  assert(strcmp(decode_one(0xF025, 0x3000).operation_name, "HALT")  == 0);
  d = decode_one(0xF02A, 0x3000);
  assert(strcmp(d.operation_name, "TRAP") == 0 && strcmp(d.operands, "x2A") == 0);

  printf("test_decode_simple_instructions passed\n");
}

/* ------------------------------------------------------------------ *
 * decode: PC-relative instructions resolve to the right target address
 * ------------------------------------------------------------------ */
static void test_decode_pc_relative(void) {
  DisassemblyInstruction d;

  /* LEA R0 at 0x3000 with offset 4 -> target 0x3000 + 1 + 4 = 0x3005 */
  d = decode_one((0xE << 12) | (0 << 9) | 4, 0x3000);
  assert(strcmp(d.operation_name, "LEA") == 0 && strcmp(d.operands, "R0") == 0);
  assert(d.has_target && d.target == 0x3005);

  /* LD R0 at 0x3002 with offset 15 -> 0x3002 + 1 + 15 = 0x3012 (the
     hello-world NEWLINE case: target = address + 1 + offset) */
  d = decode_one((0x2 << 12) | (0 << 9) | 15, 0x3002);
  assert(strcmp(d.operation_name, "LD") == 0 && d.has_target && d.target == 0x3012);

  /* a backward branch: offset -1 (0x1FF) at 0x3005 targets 0x3005 (self) */
  d = decode_one((0x0 << 12) | (0x7 << 9) | 0x1FF, 0x3005);
  assert(strcmp(d.operation_name, "BRnzp") == 0 && d.has_target && d.target == 0x3005);

  /* every nzp combination becomes the right mnemonic suffix */
  struct { uint16_t nzp; const char *operation_name; } cases[] = {
    { 0x4, "BRn" }, { 0x2, "BRz" }, { 0x1, "BRp" },
    { 0x6, "BRnz" }, { 0x5, "BRnp" }, { 0x3, "BRzp" }, { 0x7, "BRnzp" },
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    d = decode_one((0x0 << 12) | (cases[i].nzp << 9) | 0x1FF, 0x3000);
    assert(strcmp(d.operation_name, cases[i].operation_name) == 0);
  }

  /* JSR (offset mode) resolves its 11-bit target */
  d = decode_one((0x4 << 12) | (1 << 11) | 1, 0x3008);   /* offset 1 -> 0x300A */
  assert(strcmp(d.operation_name, "JSR") == 0 && d.has_target && d.target == 0x300A);

  printf("test_decode_pc_relative passed\n");
}

/* ------------------------------------------------------------------ *
 * decode: words that are NOT clean instruction encodings become data
 * ------------------------------------------------------------------ */
static void test_decode_data_fallback(void) {
  /* BR with nzp = 0 branches on nothing -> not expressible -> data */
  assert(decode_one(0x0048, 0x3000).is_data);   /* 'H' */
  assert(decode_one(0x000A, 0x3000).is_data);   /* newline literal */
  assert(decode_one(0x0000, 0x3000).is_data);   /* NUL word */

  /* the reserved opcode has no decoding at all */
  assert(decode_one(0xD000, 0x3000).is_data);   /* OP_RES */

  /* structurally-invalid encodings of real opcodes fall back too */
  assert(decode_one((0x9 << 12) | 0x000, 0x3000).is_data);   /* NOT without the 111111 tail */
  assert(decode_one((0xC << 12) | 0x001, 0x3000).is_data);   /* JMP with stray low bits */
  assert(decode_one(0x8001, 0x3000).is_data);                /* RTI with nonzero payload */
  assert(decode_one(0xF120, 0x3000).is_data);                /* TRAP with nonzero bits 11-8 */
  assert(decode_one((0x1 << 12) | 0x018, 0x3000).is_data);   /* ADD reg-mode with bits 4-3 set */

  printf("test_decode_data_fallback passed\n");
}

/* ------------------------------------------------------------------ *
 * disassemble(): the labeling pass mints and shares labels
 * ------------------------------------------------------------------ */
static void test_labeling_pass(void) {
  /* LEA R0, <0x3002> ; then two data words. The LEA's target gets a label,
     which is copied onto the LEA as its reference. */
  uint16_t words[] = {
    (uint16_t) ((0xE << 12) | (0 << 9) | 1),   /* 0x3000: LEA R0, target 0x3002 */
    (uint16_t) ((0xC << 12) | (7 << 6)),        /* 0x3001: RET */
    0x0041,                                     /* 0x3002: data 'A' */
  };
  DisassemblyInstruction *program = disassemble(words, 3, 0x3000);

  assert(program[2].label[0] != '\0');                      /* target got a label */
  assert(strcmp(program[0].reference_label, program[2].label) == 0);  /* LEA references it */
  assert(program[2].is_data);                               /* 'A' rendered as .FILL */

  char body[64];
  disassembly_instruction_body(&program[0], body, sizeof body);
  assert(strncmp(body, "LEA R0, ", 8) == 0);   /* label appended after the register */

  free(program);

  /* a PC-relative target OUTSIDE the loaded program can't be named -> the
     referencing line is demoted to data so it still re-assembles exactly. */
  uint16_t lone[] = { (uint16_t) ((0xE << 12) | (0 << 9) | 0x100) };  /* target 0x3101, out of range */
  DisassemblyInstruction *single = disassemble(lone, 1, 0x3000);
  assert(single[0].is_data && !single[0].has_target);
  free(single);

  printf("test_labeling_pass passed\n");
}

/* ------------------------------------------------------------------ *
 * round trip: disassemble(assemble(x)) re-assembles to the same machine code
 * ------------------------------------------------------------------ */
static void test_round_trip_synthetic(void) {
  /* a hand-built mix of every instruction shape plus interleaved data */
  uint16_t words[] = {
    (uint16_t) ((0xE << 12) | (0 << 9) | 3),         /* LEA R0, <0x3004 data> */
    (uint16_t) ((0x1 << 12) | (1 << 9) | (2 << 6) | (1 << 5) | 0x1B),  /* ADD R1, R2, #-5 */
    (uint16_t) ((0x9 << 12) | (3 << 9) | (4 << 6) | 0x3F),             /* NOT R3, R4 */
    (uint16_t) ((0x0 << 12) | (0x5 << 9) | 0x1FE),   /* BRnp <back to 0x3002> */
    0x0041,                                          /* 0x3004: data */
    0xF025,                                          /* HALT */
  };
  assert_round_trips(words, (int) (sizeof words / sizeof words[0]), 0x3000);

  /* exhaustive-ish: every single 16-bit value at a fixed address must survive
     one word of disassembly + reassembly (data fallback guarantees it). */
  for (int value = 0; value <= 0xFFFF; value++) {
    uint16_t word = (uint16_t) value;
    assert_round_trips(&word, 1, 0x3000);
  }

  printf("test_round_trip_synthetic passed\n");
}

/* ------------------------------------------------------------------ *
 * the ultimate test: assemble a real program, disassemble it, and confirm the
 * disassembly re-assembles to identical machine code (comment/whitespace/label
 * differences and all — only the bits are compared).
 * ------------------------------------------------------------------ */
static void test_round_trip_real_programs(void) {
  const char *programs[] = {
    "assembly/hello_world/code.asm",
    "assembly/character_count_in_string/code.asm",
  };
  for (size_t i = 0; i < sizeof programs / sizeof programs[0]; i++) {
    int count = 0;
    uint16_t origin = 0;
    uint16_t *words = assemble_file_to_words(programs[i], &count, &origin);
    assert_round_trips(words, count, origin);
    free(words);
    printf("  round-tripped %s (%d words)\n", programs[i], count);
  }
  printf("test_round_trip_real_programs passed\n");
}

int main(void) {
  test_decode_simple_instructions();
  test_decode_pc_relative();
  test_decode_data_fallback();
  test_labeling_pass();
  test_round_trip_synthetic();
  test_round_trip_real_programs();
  printf("test_disassembler passed\n");
  return 0;
}
