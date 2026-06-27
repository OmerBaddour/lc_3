#include <assert.h>
#include <stdio.h>
#include "registers.h"
#include "trap.h"

static void test_trap_getc(void) {
  uint16_t registers_local[R_COUNT] = {0};
  char data[] = "A";
  FILE *mock_stdin = fmemopen(data, sizeof(data), "r");
  trap_getc(registers_local, mock_stdin);
  assert(registers_local[R_R0] == 'A');
  fclose(mock_stdin);
}

static void test_trap_out(void) {
  uint16_t registers_local[R_COUNT] = {0};
  char buffer[2] = {0};
  registers_local[R_R0] = 'A';
  FILE *mock_stdout = fmemopen(buffer, sizeof(buffer), "w");
  trap_out(registers_local, mock_stdout);
  assert(buffer[0] == 'A');
  fclose(mock_stdout);
}

int main(void) {
  test_trap_getc();
  test_trap_out();
  printf("test_trap passed\n");
  return 0;
}
