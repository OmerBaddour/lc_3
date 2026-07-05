#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assembler/assembler.h"
#include "assembler/operation.h"

/* ------------------------------------------------------------------ *
 * helpers: build a program from source lines, assemble it, tear it down
 * ------------------------------------------------------------------ */

/* Parse `n` source lines into a program array. Blank/comment lines are
   dropped, so *count_out may be smaller than n. Caller frees via free_assembly_program. */
static AssemblyInstruction **parse_assembly_program(const char *lines[], int n, int *count_out) {
  AssemblyInstruction **program = malloc((size_t) n * sizeof *program);
  int count = 0;
  for (int i = 0; i < n; i++) {
    AssemblyInstruction *line = parse_assembly_line(lines[i]);
    if (line != NULL) {
      program[count++] = line;
    }
  }
  *count_out = count;
  return program;
}

static void free_assembly_program(AssemblyInstruction **program, int count) {
  for (int i = 0; i < count; i++) {
    free_assembly_instruction(program[i]);
  }
  free(program);
}

/* Assemble a single instruction inside a minimal .ORIG/.END wrapper and
   return its one machine word. Asserts the program assembles cleanly. */
static uint16_t assemble_one(const char *instruction) {
  const char *lines[] = { ".ORIG x3000", instruction, ".END" };
  int count = 0;
  AssemblyInstruction **program = parse_assembly_program(lines, 3, &count);
  uint16_t origin = 0;
  assert(assemble(program, count, &origin) == 1);
  assert(origin == 0x3000);
  assert(program[1]->machine_codes_length == 1);
  uint16_t word = program[1]->machine_codes[0];
  free_assembly_program(program, count);
  return word;
}

/* Assemble a whole program and assert it FAILS (returns 0). */
static void assert_assemble_fails(const char *lines[], int n) {
  int count = 0;
  AssemblyInstruction **program = parse_assembly_program(lines, n, &count);
  uint16_t origin = 0;
  assert(assemble(program, count, &origin) == 0);
  free_assembly_program(program, count);
}

/* ------------------------------------------------------------------ *
 * parse_assembly_line
 * ------------------------------------------------------------------ */
static void test_parse_assembly_line(void) {
  AssemblyInstruction *line;

  /* blank and comment-only lines vanish */
  assert(parse_assembly_line("") == NULL);
  assert(parse_assembly_line("   \t  \n") == NULL);
  assert(parse_assembly_line("; just a comment") == NULL);

  /* plain instruction: no label, three register operands */
  line = parse_assembly_line("ADD R0, R1, R2");
  assert(line->label == NULL);
  assert(strcmp(line->operation_name, "ADD") == 0);
  assert(line->operands_length == 3);
  assert(line->operands[0].type == OPERAND_REGISTER && line->operands[0].as.reg->code == 0);
  assert(line->operands[1].type == OPERAND_REGISTER && line->operands[1].as.reg->code == 1);
  assert(line->operands[2].type == OPERAND_REGISTER && line->operands[2].as.reg->code == 2);
  free_assembly_instruction(line);

  /* label + negative immediate; trailing comment stripped */
  line = parse_assembly_line("LOOP ADD R0, R0, #-5 ; count down");
  assert(strcmp(line->label, "LOOP") == 0);
  assert(strcmp(line->operation_name, "ADD") == 0);
  assert(line->operands[2].type == OPERAND_INTEGER && line->operands[2].as.integer == -5);
  free_assembly_instruction(line);

  /* case-insensitive mnemonics and registers; every immediate spelling */
  line = parse_assembly_line("add r0, r1, x1F");
  assert(strcmp(line->operation_name, "add") == 0);   /* original casing kept */
  assert(line->operands[0].type == OPERAND_REGISTER && line->operands[0].as.reg->code == 0);
  assert(line->operands[2].type == OPERAND_INTEGER && line->operands[2].as.integer == 31);
  free_assembly_instruction(line);

  line = parse_assembly_line("ADD R0, R0, 0x1F");
  assert(line->operands[2].type == OPERAND_INTEGER && line->operands[2].as.integer == 31);
  free_assembly_instruction(line);

  line = parse_assembly_line("ADD R0, R0, 42");
  assert(line->operands[2].type == OPERAND_INTEGER && line->operands[2].as.integer == 42);
  free_assembly_instruction(line);

  /* label operand (not a register, not a number) */
  line = parse_assembly_line("BR LOOP");
  assert(line->operands_length == 1);
  assert(line->operands[0].type == OPERAND_LABEL);
  assert(strcmp(line->operands[0].as.label, "LOOP") == 0);
  free_assembly_instruction(line);

  /* BR condition-code suffixes are mnemonics ... */
  line = parse_assembly_line("BRnzp SOMEWHERE");
  assert(line->label == NULL);
  assert(strcmp(line->operation_name, "BRnzp") == 0);
  free_assembly_instruction(line);

  /* ... but BR + other letters is a label starting a line */
  line = parse_assembly_line("BRX ADD R0, R0, #1");
  assert(strcmp(line->label, "BRX") == 0);
  assert(strcmp(line->operation_name, "ADD") == 0);
  free_assembly_instruction(line);

  /* .STRINGZ keeps spaces and commas inside the quotes as ONE operand */
  line = parse_assembly_line("HELLO .STRINGZ \"Hello, World!\"");
  assert(strcmp(line->label, "HELLO") == 0);
  assert(line->operands_length == 1);
  assert(line->operands[0].type == OPERAND_STRING);
  assert(strcmp(line->operands[0].as.string, "Hello, World!") == 0);
  free_assembly_instruction(line);

  /* a label sitting alone on its line */
  line = parse_assembly_line("TARGET");
  assert(strcmp(line->label, "TARGET") == 0);
  assert(line->operation_name == NULL);
  free_assembly_instruction(line);

  printf("test_parse_assembly_line passed\n");
}

/* ------------------------------------------------------------------ *
 * encoding: register-form and immediate-form instructions
 * ------------------------------------------------------------------ */
static void test_encode_simple_instructions(void) {
  /* ADD / AND, register form and imm5 form */
  assert(assemble_one("ADD R0, R1, R2") == ((0x1 << 12) | (0 << 9) | (1 << 6) | 2));
  assert(assemble_one("AND R1, R2, R3") == ((0x5 << 12) | (1 << 9) | (2 << 6) | 3));
  /* the imm5 negative case: -5 & 0x1F == 0x1B (two's complement in 5 bits) */
  assert(assemble_one("ADD R0, R0, #-5") == 0x103B);
  /* imm5 boundaries */
  assert(assemble_one("ADD R0, R0, #15")  == ((0x1 << 12) | (1 << 5) | 0x0F));
  assert(assemble_one("ADD R0, R0, #-16") == ((0x1 << 12) | (1 << 5) | 0x10));
  assert(assemble_one("AND R0, R0, #0")   == ((0x5 << 12) | (1 << 5)));

  assert(assemble_one("NOT R3, R4") == ((0x9 << 12) | (3 << 9) | (4 << 6) | 0x3F));

  assert(assemble_one("JMP R2")  == ((0xC << 12) | (2 << 6)));
  assert(assemble_one("RET")     == ((0xC << 12) | (7 << 6)));
  assert(assemble_one("JSRR R6") == ((0x4 << 12) | (6 << 6)));

  /* offset6 in LDR/STR, including negative */
  assert(assemble_one("LDR R1, R2, #-1") == ((0x6 << 12) | (1 << 9) | (2 << 6) | 0x3F));
  assert(assemble_one("STR R3, R4, #4")  == ((0x7 << 12) | (3 << 9) | (4 << 6) | 4));

  assert(assemble_one("TRAP x21") == 0xF021);
  assert(assemble_one("RTI")      == 0x8000);

  /* trap aliases */
  assert(assemble_one("GETC")  == 0xF020);
  assert(assemble_one("OUT")   == 0xF021);
  assert(assemble_one("PUTS")  == 0xF022);
  assert(assemble_one("IN")    == 0xF023);
  assert(assemble_one("PUTSP") == 0xF024);
  assert(assemble_one("HALT")  == 0xF025);

  printf("test_encode_simple_instructions passed\n");
}

/* ------------------------------------------------------------------ *
 * encoding: PC-relative instructions (labels forward and backward)
 * ------------------------------------------------------------------ */
static void test_encode_pc_relative(void) {
  const char *lines[] = {
    ".ORIG x3000",
    "LOOP ADD R0, R0, #1",   /* 0x3000 */
    "BRzp LOOP",             /* 0x3001: offset 0x3000 - 0x3002 = -2 */
    "BR LOOP",               /* 0x3002: bare BR = BRnzp, offset -3  */
    "LD  R1, DATA",          /* 0x3003: forward, offset 0x300A - 0x3004 = 6 */
    "LDI R2, DATA",          /* 0x3004: offset 5 */
    "LEA R3, DATA",          /* 0x3005: offset 4 */
    "ST  R4, DATA",          /* 0x3006: offset 3 */
    "STI R5, DATA",          /* 0x3007: offset 2 */
    "JSR DATA",              /* 0x3008: offset 1 (11-bit field) */
    "JSR LOOP",              /* 0x3009: backward, offset -10 */
    "DATA .FILL x1234",      /* 0x300A */
    ".END",
  };
  int count = 0;
  AssemblyInstruction **program = parse_assembly_program(lines, sizeof lines / sizeof lines[0], &count);
  uint16_t origin = 0;
  assert(assemble(program, count, &origin) == 1);
  assert(origin == 0x3000);

  uint16_t loop = program[1]->address;   /* LOOP */
  uint16_t data = program[11]->address;  /* DATA */
  assert(loop == 0x3000);
  assert(data == 0x300A);

  /* offset = target - (instruction address + 1), masked to the field width */
  #define OFF9(target, instr)  ((uint16_t) (((target) - ((instr) + 1)) & 0x1FF))
  #define OFF11(target, instr) ((uint16_t) (((target) - ((instr) + 1)) & 0x7FF))

  assert(program[2]->machine_codes[0]  == ((0x0 << 12) | (0x3 << 9) | OFF9(loop, 0x3001)));  /* BRzp: z|p */
  assert(program[3]->machine_codes[0]  == ((0x0 << 12) | (0x7 << 9) | OFF9(loop, 0x3002)));  /* BR = nzp */
  assert(program[4]->machine_codes[0]  == ((0x2 << 12) | (1 << 9) | OFF9(data, 0x3003)));    /* LD  */
  assert(program[5]->machine_codes[0]  == ((0xA << 12) | (2 << 9) | OFF9(data, 0x3004)));    /* LDI */
  assert(program[6]->machine_codes[0]  == ((0xE << 12) | (3 << 9) | OFF9(data, 0x3005)));    /* LEA */
  assert(program[7]->machine_codes[0]  == ((0x3 << 12) | (4 << 9) | OFF9(data, 0x3006)));    /* ST  */
  assert(program[8]->machine_codes[0]  == ((0xB << 12) | (5 << 9) | OFF9(data, 0x3007)));    /* STI */
  assert(program[9]->machine_codes[0]  == ((0x4 << 12) | (1 << 11) | OFF11(data, 0x3008)));  /* JSR fwd */
  assert(program[10]->machine_codes[0] == ((0x4 << 12) | (1 << 11) | OFF11(loop, 0x3009)));  /* JSR back */
  assert(program[11]->machine_codes[0] == 0x1234);                                           /* .FILL */

  free_assembly_program(program, count);

  /* every BR condition-code suffix maps to its nzp bits */
  struct { const char *suffix; uint16_t nzp; } cc[] = {
    { "n", 0x4 }, { "z", 0x2 }, { "p", 0x1 },
    { "nz", 0x6 }, { "np", 0x5 }, { "zp", 0x3 }, { "nzp", 0x7 }, { "", 0x7 },
  };
  for (size_t i = 0; i < sizeof cc / sizeof cc[0]; i++) {
    char source[64];
    snprintf(source, sizeof source, "HERE BR%s HERE", cc[i].suffix);
    /* self-branch: offset = HERE - (HERE + 1) = -1 -> 0x1FF */
    assert(assemble_one(source) == ((0x0 << 12) | (cc[i].nzp << 9) | 0x1FF));
  }

  printf("test_encode_pc_relative passed\n");
}

/* ------------------------------------------------------------------ *
 * directives and pass-1 addressing
 * ------------------------------------------------------------------ */
static void test_directives_and_addressing(void) {
  const char *lines[] = {
    ".ORIG x3000",
    "START ADD R0, R0, #0",        /* 0x3000, 1 word */
    "BUF   .BLKW 3",               /* 0x3001, 3 zero words */
    "MSG   .STRINGZ \"Hi\"",       /* 0x3004, 3 words: 'H' 'i' 0 */
    "PTR   .FILL START",           /* 0x3007: label .FILL stores the ABSOLUTE address */
    "NUM   .FILL #42",             /* 0x3008 */
    "ALONE",                       /* label-only: binds to the NEXT word's address */
    "LD R1, ALONE",                /* 0x3009 */
    ".END",
  };
  int count = 0;
  AssemblyInstruction **program = parse_assembly_program(lines, sizeof lines / sizeof lines[0], &count);
  uint16_t origin = 0;
  assert(assemble(program, count, &origin) == 1);

  /* location counter walks multi-word directives correctly */
  assert(program[1]->address == 0x3000);  /* START */
  assert(program[2]->address == 0x3001);  /* BUF   */
  assert(program[3]->address == 0x3004);  /* MSG   */
  assert(program[4]->address == 0x3007);  /* PTR   */
  assert(program[5]->address == 0x3008);  /* NUM   */
  assert(program[6]->address == 0x3009);  /* ALONE (label-only line) */
  assert(program[7]->address == 0x3009);  /* LD shares ALONE's address */

  /* .BLKW: n zeroed words */
  assert(program[2]->machine_codes_length == 3);
  assert(program[2]->machine_codes[0] == 0 && program[2]->machine_codes[2] == 0);

  /* .STRINGZ: one word per char plus the NUL terminator */
  assert(program[3]->machine_codes_length == 3);
  assert(program[3]->machine_codes[0] == 'H');
  assert(program[3]->machine_codes[1] == 'i');
  assert(program[3]->machine_codes[2] == 0);

  /* .FILL: absolute label address (not PC-relative), and integer literal */
  assert(program[4]->machine_codes_length == 1);
  assert(program[4]->machine_codes[0] == 0x3000);
  assert(program[5]->machine_codes[0] == 42);

  /* label-only ALONE resolves to the following instruction: LD R1, ALONE at
     0x3009 branches to itself -> offset -1 */
  assert(program[7]->machine_codes[0] == ((0x2 << 12) | (1 << 9) | 0x1FF));

  free_assembly_program(program, count);
  printf("test_directives_and_addressing passed\n");
}

/* ------------------------------------------------------------------ *
 * error paths: assemble() must return 0
 * ------------------------------------------------------------------ */
static void test_errors(void) {
  fprintf(stderr, "--- expected errors below (exercising failure paths) ---\n");

  /* missing .ORIG */
  {
    const char *lines[] = { "ADD R0, R0, #1", ".END" };
    assert_assemble_fails(lines, 2);
  }

  /* undefined label */
  {
    const char *lines[] = { ".ORIG x3000", "BR NOWHERE", ".END" };
    assert_assemble_fails(lines, 3);
  }

  /* branch target beyond the 9-bit offset range (needs > 256 words of gap) */
  {
    const char *lines[] = {
      ".ORIG x3000", "FAR ADD R0, R0, #0", ".BLKW 300", "BR FAR", ".END",
    };
    assert_assemble_fails(lines, 5);
  }

  /* immediates that would silently wrap without range checks */
  {
    const char *lines[] = { ".ORIG x3000", "ADD R0, R0, #16", ".END" };
    assert_assemble_fails(lines, 3);
  }
  {
    const char *lines[] = { ".ORIG x3000", "ADD R0, R0, #-17", ".END" };
    assert_assemble_fails(lines, 3);
  }
  {
    const char *lines[] = { ".ORIG x3000", "LDR R0, R1, #32", ".END" };
    assert_assemble_fails(lines, 3);
  }
  {
    const char *lines[] = { ".ORIG x3000", "STR R0, R1, #-33", ".END" };
    assert_assemble_fails(lines, 3);
  }
  {
    const char *lines[] = { ".ORIG x3000", "TRAP x100", ".END" };
    assert_assemble_fails(lines, 3);
  }

  /* ... while the in-range boundaries still assemble (see simple tests) */
  assert(assemble_one("LDR R0, R1, #31")  == ((0x6 << 12) | (1 << 6) | 0x1F));
  assert(assemble_one("LDR R0, R1, #-32") == ((0x6 << 12) | (1 << 6) | 0x20));

  fprintf(stderr, "--- end of expected errors ---\n");
  printf("test_errors passed\n");
}

/* ------------------------------------------------------------------ *
 * end to end: hello world source -> exact object-file bytes
 * ------------------------------------------------------------------ */
static void test_hello_world_object_file(void) {
  FILE *input = fopen("assembly/hello_world/code.asm", "r");
  assert(input != NULL);  /* run from the repo root (as `make test` does) */

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
  fclose(input);

  uint16_t origin = 0;
  assert(assemble(program, count, &origin) == 1);

  const char *obj_path = "tests/test_assembler_tmp.obj";
  assert(write_object_file(obj_path, origin, program, count) == 1);
  free_assembly_program(program, count);

  /* the known-good object file, hand-assembled (big-endian):
       0x3000  origin
       0xE004  LEA  R0, HELLO_STR   ; DR=R0, PCoffset9 = 0x3005 - 0x3001 = 4
       0xF022  PUTS
       0x200F  LD   R0, NEWLINE     ; DR=R0, PCoffset9 = 0x3012 - 0x3003 = 15
       0xF021  OUT
       0xF025  HALT
       "Hello World!" one char per word, then the NUL word
       0x000A  NEWLINE .FILL 0x000A */
  const uint16_t expected_words[] = {
    0x3000, 0xE004, 0xF022, 0x200F, 0xF021, 0xF025,
    'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', 0x0000,
    0x000A,
  };
  const int n = (int) (sizeof expected_words / sizeof expected_words[0]);

  FILE *obj = fopen(obj_path, "rb");
  assert(obj != NULL);
  for (int i = 0; i < n; i++) {
    int hi = fgetc(obj);
    int lo = fgetc(obj);
    assert(hi != EOF && lo != EOF);
    assert(((hi << 8) | lo) == expected_words[i]);
  }
  assert(fgetc(obj) == EOF);  /* nothing extra after the last word */
  fclose(obj);
  remove(obj_path);

  printf("test_hello_world_object_file passed\n");
}

/* ------------------------------------------------------------------ *
 * assembly_operation_by_name: mnemonic families map to their base op
 * ------------------------------------------------------------------ */
static void test_assembly_operation_by_name(void) {
  /* exact canonical names */
  assert(assembly_operation_by_name("ADD")->operation == &OPERATION_ADD);
  assert(assembly_operation_by_name("TRAP")->operation == &OPERATION_TRAP);

  /* families the VM never sees fold onto their base opcode. NOTE the argument
     is already upper-cased by the contract (the assembler upper-cases first). */
  assert(assembly_operation_by_name("BR")->operation    == &OPERATION_BR);
  assert(assembly_operation_by_name("BRNZP")->operation == &OPERATION_BR);
  assert(assembly_operation_by_name("BRZ")->operation   == &OPERATION_BR);
  assert(assembly_operation_by_name("RET")->operation   == &OPERATION_JMP);
  assert(assembly_operation_by_name("JSRR")->operation  == &OPERATION_JSR);

  /* non-operations: labels, trap aliases, directives, RES */
  assert(assembly_operation_by_name("BRX") == NULL);   /* B,R then a non-cc letter -> label */
  assert(assembly_operation_by_name("LOOP") == NULL);
  assert(assembly_operation_by_name("GETC") == NULL);  /* trap alias, not an operation */
  assert(assembly_operation_by_name(".FILL") == NULL);
  assert(assembly_operation_by_name("RES") == NULL);

  printf("test_assembly_operation_by_name passed\n");
}

int main(void) {
  test_assembly_operation_by_name();
  test_parse_assembly_line();
  test_encode_simple_instructions();
  test_encode_pc_relative();
  test_directives_and_addressing();
  test_errors();
  test_hello_world_object_file();
  printf("test_assembler passed\n");
  return 0;
}
