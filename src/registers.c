#include <string.h>
#include "registers.h"

const Register REGISTER_R0               = { .code = 0,   .name = "R0" };
const Register REGISTER_R1               = { .code = 1,   .name = "R1" };
const Register REGISTER_R2               = { .code = 2,   .name = "R2" };
const Register REGISTER_R3               = { .code = 3,   .name = "R3" };
const Register REGISTER_R4               = { .code = 4,   .name = "R4" };
const Register REGISTER_R5               = { .code = 5,   .name = "R5" };
const Register REGISTER_R6               = { .code = 6,   .name = "R6" };
const Register REGISTER_R7               = { .code = 7,   .name = "R7" };
const Register REGISTER_PROGRAM_COUNTER  = { .code = 8,   .name = "PROGRAM_COUNTER" };
const Register REGISTER_PROCESSOR_STATUS = { .code = 9,   .name = "PROCESSOR_STATUS" };

const Register *const REGISTERS[REGISTER_COUNT] = {
    &REGISTER_R0,
    &REGISTER_R1,
    &REGISTER_R2,
    &REGISTER_R3,
    &REGISTER_R4,
    &REGISTER_R5,
    &REGISTER_R6,
    &REGISTER_R7,
    &REGISTER_PROGRAM_COUNTER,
    &REGISTER_PROCESSOR_STATUS,
};

const Register *register_by_code(uint16_t code) {
  if (code >= REGISTER_COUNT) {
    return NULL;
  }
  return REGISTERS[code];
}

const Register *register_by_name(const char *name) {
  for (uint16_t i = 0; i < REGISTER_COUNT; i++) {
    if (strcmp(REGISTERS[i]->name, name) == 0) {
      return REGISTERS[i];
    }
  }
  return NULL;
}

/* the one real definition of the register file (runtime values, indexed by code) */
uint16_t registers[REGISTER_COUNT] = {0};

void update_register_condition_flags(
    uint16_t registers[],
    uint16_t destination_register
) {
  if (registers[destination_register] == 0) {
    registers[REGISTER_PROCESSOR_STATUS.code] = CONDITION_FLAG_ZERO;
  } else if (registers[destination_register] >> (16 - 1)) {  /* negative means most significant bit is a 1 */
    registers[REGISTER_PROCESSOR_STATUS.code] = CONDITION_FLAG_NEGATIVE;
  } else {
    registers[REGISTER_PROCESSOR_STATUS.code] = CONDITION_FLAG_POSITIVE;
  }
}
