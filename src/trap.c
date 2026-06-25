#include <stdio.h>
#include <stdint.h>
#include "registers.h"
#include "trap.h"

void trap_getc(uint16_t registers[]) {
  int character = getc(stdin);
  registers[R_R0] = (uint16_t)character;
  update_register_condition_flags(R_R0);
}
