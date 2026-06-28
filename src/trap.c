#include <stdio.h>
#include <stdint.h>
#include "registers.h"
#include "trap.h"

void trap_getc(uint16_t registers[], FILE *file) {
  int character = getc(file);
  registers[R_R0] = (uint16_t)character;
  update_register_condition_flags(registers, R_R0);
}

void trap_out(uint16_t registers[], FILE *file) {
  putc((char)registers[R_R0], file);
  fflush(file);
}

void trap_puts(
    uint16_t memory[],
    uint16_t registers[],
    FILE *file
) {
  /* one character per word */
  uint16_t* character = memory + registers[R_R0];
  while (*character) {
    putc((char)*character, file);
    ++character;
  }
  fflush(file);
}

void trap_in(
    uint16_t registers[],
    FILE *file_input,
    FILE *file_output
) {
  fprintf(file_output, "Enter character: ");
  int character = getc(file_input);
  registers[R_R0] = (uint16_t)character;
  putc((char)character, file_output);
  fflush(file_output);
}
