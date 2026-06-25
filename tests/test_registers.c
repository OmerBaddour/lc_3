#include <assert.h>
#include <stdio.h>
#include "registers.h"

static void test_set_positive_flag(void) {
  uint16_t destination_register = R_R0;
  registers[destination_register] = 0x0FFF;
  update_register_condition_flags(destination_register);
  assert(registers[R_COND] == FL_POS);
}

static void test_set_zero_flag(void) {
  uint16_t destination_register = R_R0;
  registers[destination_register] = 0;
  update_register_condition_flags(destination_register);
  assert(registers[R_COND] == FL_ZRO);
}

static void test_set_negative_flag(void) {
  uint16_t destination_register = R_R0;
  registers[destination_register] = 0xF000;
  update_register_condition_flags(destination_register);
  assert(registers[R_COND] == FL_NEG);
}

int main(void) {
  test_set_positive_flag();
  test_set_zero_flag();
  test_set_negative_flag();
  printf("test_registers passed\n");
  return 0;
}