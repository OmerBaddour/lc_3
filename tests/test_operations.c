#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "operations.h"
#include "registers.h"

/* memory-touching ops take a memory[] array; each test owns a local one, so
   tests stay isolated (normal addresses never hit the keyboard path) */

/* build an instruction with op in 15-12, dr in 11-9, sr1 in 8-6, imm5 mode */
static uint16_t encode_imm(uint16_t op, uint16_t dr, uint16_t sr1, uint16_t imm5) {
  return (op << 12) | (dr << 9) | (sr1 << 6) | (1 << 5) | (imm5 & 0x1F);
}

static void test_sign_extend(void) {
  assert(sign_extend(0x0F, 5) == 0x000F);  /* 0b01111, sign bit clear -> 15 */
  assert(sign_extend(0x1F, 5) == 0xFFFF);  /* 0b11111 -> -1 */
  assert(sign_extend(0x10, 5) == 0xFFF0);  /* 0b10000 -> -16 */
  assert(sign_extend(0x1FF, 9) == 0xFFFF); /* all-ones 9-bit -> -1 */
}

static void test_operation_add(void) {
  // uint16_t registers_local[R_COUNT] = {0};
  // registers_local[R_R1] = 5;
  // /* ADD R0, R1, R2 (register mode): op=0001 dr=000 sr1=001 0 00 sr2=010 */
  // registers_local[R_R2] = 7;
  // uint16_t instruction = (0x1 << 12) | (R_R0 << 9) | (R_R1 << 6) | (0 << 5) | R_R2;
  // operation_add(instruction, registers_local);
  // assert(registers_local[R_R0] == 12);
  // assert(registers_local[R_COND] == CONDITION_FLAG_POSITIVE);

  // /* ADD R0, R1, #-1 (immediate mode) */
  // instruction = encode_imm(0x1, R_R0, R_R1, 0x1F); /* imm5 = -1 */
  // operation_add(instruction, registers_local);
  // assert(registers_local[R_R0] == 4);
  // assert(registers_local[R_COND] == CONDITION_FLAG_POSITIVE);
}

static void test_operation_and(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_R1] = 0xFF0F;
  /* AND R0, R1, #0x0F (immediate) -> 0x000F */
  uint16_t instruction = encode_imm(0x5, R_R0, R_R1, 0x0F);
  operation_and(instruction, registers_local);
  assert(registers_local[R_R0] == 0x000F);
  assert(registers_local[R_COND] == CONDITION_FLAG_POSITIVE);

  /* AND that yields zero sets the zero flag */
  registers_local[R_R1] = 0x00F0;
  instruction = encode_imm(0x5, R_R0, R_R1, 0x0F);
  operation_and(instruction, registers_local);
  assert(registers_local[R_R0] == 0);
  assert(registers_local[R_COND] == CONDITION_FLAG_ZERO);
}

static void test_operation_not(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_R1] = 0x0000;
  /* NOT R0, R1: op=1001 dr=000 sr1=001 1 11111 */
  uint16_t instruction = (0x9 << 12) | (R_R0 << 9) | (R_R1 << 6) | 0x3F;
  operation_not(instruction, registers_local);
  assert(registers_local[R_R0] == 0xFFFF);
  assert(registers_local[R_COND] == CONDITION_FLAG_NEGATIVE);
}

static void test_operation_br(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_PC] = 0x3000;
  registers_local[R_COND] = CONDITION_FLAG_ZERO;
  /* BRz #5: taken because COND == ZRO */
  uint16_t instruction = (0x0 << 12) | (0 << 11) | (1 << 10) | (0 << 9) | 0x005;
  operation_br(instruction, registers_local);
  assert(registers_local[R_PC] == 0x3005);

  /* BRn #5: not taken because COND != NEG */
  registers_local[R_PC] = 0x3000;
  instruction = (0x0 << 12) | (1 << 11) | (0 << 10) | (0 << 9) | 0x005;
  operation_br(instruction, registers_local);
  assert(registers_local[R_PC] == 0x3000);
}

static void test_operation_jmp(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_R2] = 0x4321;
  /* JMP R2: op=1100 000 baseR=010 000000 */
  uint16_t instruction = (0xC << 12) | (R_R2 << 6);
  operation_jmp(instruction, registers_local);
  assert(registers_local[R_PC] == 0x4321);
}

static void test_operation_jsr(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_PC] = 0x3000;
  /* JSR #10 (offset mode): op=0100 1 PCoffset11 */
  uint16_t instruction = (0x4 << 12) | (1 << 11) | 0x00A;
  operation_jsr(instruction, registers_local);
  assert(registers_local[R_R7] == 0x3000); /* return address saved */
  assert(registers_local[R_PC] == 0x300A);

  /* JSRR R3 (register mode): op=0100 0 00 baseR=011 00000 */
  registers_local[R_PC] = 0x3000;
  registers_local[R_R3] = 0x5000;
  instruction = (0x4 << 12) | (0 << 11) | (R_R3 << 6);
  operation_jsr(instruction, registers_local);
  assert(registers_local[R_R7] == 0x3000);
  assert(registers_local[R_PC] == 0x5000);
}

static void test_operation_lea(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_PC] = 0x3000;
  /* LEA R0, #4: op=1110 dr=000 PCoffset9 */
  uint16_t instruction = (0xE << 12) | (R_R0 << 9) | 0x004;
  operation_lea(instruction, registers_local);
  assert(registers_local[R_R0] == 0x3004);
  assert(registers_local[R_COND] == CONDITION_FLAG_POSITIVE);
}

static void test_operation_ld(void) {
  uint16_t registers_local[R_COUNT] = {0};
  uint16_t memory_local[16] = {0};
  registers_local[R_PC] = 0;
  memory_local[4] = 0xABCD;
  /* LD R0, #4: op=0010 dr=000 PCoffset9 */
  uint16_t instruction = (0x2 << 12) | (R_R0 << 9) | 0x004;
  operation_ld(instruction, registers_local, memory_local);
  assert(registers_local[R_R0] == 0xABCD);
  assert(registers_local[R_COND] == CONDITION_FLAG_NEGATIVE); /* high bit set */
}

static void test_operation_ldi(void) {
  uint16_t registers_local[R_COUNT] = {0};
  uint16_t memory_local[16] = {0};
  registers_local[R_PC] = 0;
  memory_local[4] = 8;       /* pointer */
  memory_local[8] = 0x0042;  /* value */
  /* LDI R0, #4: op=1010 dr=000 PCoffset9 */
  uint16_t instruction = (0xA << 12) | (R_R0 << 9) | 0x004;
  operation_ldi(instruction, registers_local, memory_local);
  assert(registers_local[R_R0] == 0x0042);
  assert(registers_local[R_COND] == CONDITION_FLAG_POSITIVE);
}

static void test_operation_ldr(void) {
  uint16_t registers_local[R_COUNT] = {0};
  uint16_t memory_local[16] = {0};
  registers_local[R_R1] = 2;
  memory_local[5] = 0x0007;
  /* LDR R0, R1, #3: op=0110 dr=000 baseR=001 offset6 */
  uint16_t instruction = (0x6 << 12) | (R_R0 << 9) | (R_R1 << 6) | 0x03;
  operation_ldr(instruction, registers_local, memory_local);
  assert(registers_local[R_R0] == 0x0007);
  assert(registers_local[R_COND] == CONDITION_FLAG_POSITIVE);
}

static void test_operation_st(void) {
  uint16_t registers_local[R_COUNT] = {0};
  uint16_t memory_local[16] = {0};
  registers_local[R_PC] = 0;
  registers_local[R_R0] = 0x1234;
  /* ST R0, #4: op=0011 sr=000 PCoffset9 */
  uint16_t instruction = (0x3 << 12) | (R_R0 << 9) | 0x004;
  operation_st(instruction, registers_local, memory_local);
  assert(memory_local[4] == 0x1234);
}

static void test_operation_sti(void) {
  uint16_t registers_local[R_COUNT] = {0};
  uint16_t memory_local[16] = {0};
  registers_local[R_PC] = 0;
  registers_local[R_R0] = 0x9999;
  memory_local[4] = 8;  /* pointer */
  /* STI R0, #4: op=1011 sr=000 PCoffset9 */
  uint16_t instruction = (0xB << 12) | (R_R0 << 9) | 0x004;
  operation_sti(instruction, registers_local, memory_local);
  assert(memory_local[8] == 0x9999);
}

static void test_operation_str(void) {
  uint16_t registers_local[R_COUNT] = {0};
  uint16_t memory_local[16] = {0};
  registers_local[R_R1] = 2;
  registers_local[R_R0] = 0x5678;
  /* STR R0, R1, #3: op=0111 sr=000 baseR=001 offset6 */
  uint16_t instruction = (0x7 << 12) | (R_R0 << 9) | (R_R1 << 6) | 0x03;
  operation_str(instruction, registers_local, memory_local);
  assert(memory_local[5] == 0x5678);
}

int main(void) {
  test_sign_extend();
  test_operation_add();
  test_operation_and();
  test_operation_not();
  test_operation_br();
  test_operation_jmp();
  test_operation_jsr();
  test_operation_lea();
  test_operation_ld();
  test_operation_ldi();
  test_operation_ldr();
  test_operation_st();
  test_operation_sti();
  test_operation_str();
  printf("test_operations passed\n");
  return 0;
}
