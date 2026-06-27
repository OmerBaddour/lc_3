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

int main(void) {
  test_trap_getc();
  printf("test_trap passed\n");
  return 0;
}
