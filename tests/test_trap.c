#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "registers.h"
#include "trap.h"

static void test_trap_getc(void) {
  uint16_t registers_local[REGISTER_COUNT] = {0};
  char data[] = "A";
  FILE *mock_stdin = fmemopen(data, sizeof(data), "r");
  trap_getc(registers_local, mock_stdin);
  assert(registers_local[REGISTER_R0.code] == 'A');
  fclose(mock_stdin);
}

static void test_trap_out(void) {
  uint16_t registers_local[REGISTER_COUNT] = {0};
  char buffer[2] = {0};
  registers_local[REGISTER_R0.code] = 'A';
  FILE *mock_stdout = fmemopen(buffer, sizeof(buffer), "w");
  trap_out(registers_local, mock_stdout);
  assert(buffer[0] == 'A');
  fclose(mock_stdout);
}

static void test_trap_puts(void) {
  const char expected[] = "Hello";
  /* one char per word; sizeof includes the '\0', so the last word is the null terminator */
  uint16_t memory_local[sizeof expected] = {0};
  for (size_t i = 0; i < sizeof expected; ++i) {
    memory_local[i] = (uint16_t)expected[i];
  }

  uint16_t registers_local[REGISTER_COUNT] = {0};
  registers_local[REGISTER_R0.code] = 0; /* string starts at address 0 */

  char buffer[64] = {0};
  FILE *mock_stdout = fmemopen(buffer, sizeof(buffer), "w");
  trap_puts(memory_local, registers_local, mock_stdout);
  long written = ftell(mock_stdout); /* exact number of bytes emitted */
  fclose(mock_stdout);

  assert(written == (long)strlen(expected));
  assert(memcmp(buffer, expected, written) == 0); /* bounded by written, never over-reads */
}

static void test_trap_in(void) {
  uint16_t registers_local[REGISTER_COUNT] = {0};
  char buffer_stdin[] = "A";
  FILE *mock_stdin = fmemopen(buffer_stdin, sizeof(buffer_stdin), "r");
  char buffer_stdout[64] = {0};
  FILE *mock_stdout = fmemopen(buffer_stdout, sizeof(buffer_stdout), "w");
  trap_in(registers_local, mock_stdin, mock_stdout);
  
  assert(registers_local[REGISTER_R0.code] == (uint16_t)'A');
  assert(strcmp(buffer_stdout, "Enter character: A") == 0);

  fclose(mock_stdin);
  fclose(mock_stdout);
}

static void test_trap_putsp(void) {
  const char expected[] = "Hello";
  /* two chars per word; sizeof includes the '\0', so the last word is the null terminator */
  uint16_t memory_local[sizeof expected] = {0};
  size_t index_expected = 0;
  size_t index_memory = 0;
  while (index_expected < sizeof expected) {
    if (index_expected % 2 == 0) {
      memory_local[index_memory] = expected[index_expected];
    } else {
      memory_local[index_memory] = memory_local[index_memory] | (expected[index_expected] << 8);
      index_memory += 1;
    }
    index_expected += 1;
  }

  uint16_t registers_local[REGISTER_COUNT] = {0};
  registers_local[REGISTER_R0.code] = 0; /* string starts at address 0 */

  char buffer[64] = {0};
  FILE *mock_stdout = fmemopen(buffer, sizeof(buffer), "w");
  trap_putsp(memory_local, registers_local, mock_stdout);
  long written = ftell(mock_stdout); /* exact number of bytes emitted */
  fclose(mock_stdout);

  assert(written == (long)strlen(expected));
  assert(memcmp(buffer, expected, written) == 0); /* bounded by written, never over-reads */
}

int main(void) {
  test_trap_getc();
  test_trap_out();
  test_trap_puts();
  test_trap_in();
  test_trap_putsp();
  printf("test_trap passed\n");
  return 0;
}
