#include <stdio.h>
#include <stdint.h>
#include "core/registers.h"
#include "virtual_machine/trap.h"

void trap_getc(uint16_t registers[], FILE *file) {
  int character = getc(file);
  registers[REGISTER_R0.code] = (uint16_t)character;
  update_register_condition_flags(registers, REGISTER_R0.code);
}

void trap_out(uint16_t registers[], FILE *file) {
  putc((char)registers[REGISTER_R0.code], file);
  fflush(file);
}

void trap_puts(
    uint16_t memory[],
    uint16_t registers[],
    FILE *file
) {
  /* one character per word */
  uint16_t* character = memory + registers[REGISTER_R0.code];
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
  registers[REGISTER_R0.code] = (uint16_t)character;
  putc((char)character, file_output);
  fflush(file_output);
}

void trap_putsp(
  uint16_t memory[],
  uint16_t registers[],
  FILE *file
) {
  /* two characters per word: low byte first, then high byte */
  uint16_t* word = memory + registers[REGISTER_R0.code];
  while (*word) {
    char first_character = (*word) & 0xFF;
    putc(first_character, file);
    char second_character = (*word) >> 8;
    if (second_character) {
      putc(second_character, file);
    }
    ++word;
  }
  fflush(file);
}
