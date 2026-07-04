#include <assert.h>
#include <stdio.h>
#include "registers.h"

static void test_set_positive_flag(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_R0] = 0x0FFF;
  update_register_condition_flags(registers_local, R_R0);
  assert(registers_local[R_COND] == CONDITION_FLAG_POSITIVE);
}

static void test_set_zero_flag(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_R0] = 0;
  update_register_condition_flags(registers_local, R_R0);
  assert(registers_local[R_COND] == CONDITION_FLAG_ZERO);
}

static void test_set_negative_flag(void) {
  uint16_t registers_local[R_COUNT] = {0};
  registers_local[R_R0] = 0xF000;
  update_register_condition_flags(registers_local, R_R0);
  assert(registers_local[R_COND] == CONDITION_FLAG_NEGATIVE);
}

int main(void) {
  test_set_positive_flag();
  test_set_zero_flag();
  test_set_negative_flag();
  printf("test_registers passed\n");
  return 0;
}
