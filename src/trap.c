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