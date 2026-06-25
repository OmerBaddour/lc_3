#include "registers.h"

/* the one real definition of the register file */
uint16_t registers[R_COUNT];

void update_register_condition_flags(uint16_t destination_register) {
  if (registers[destination_register] == 0) {
    registers[R_COND] = FL_ZRO;
  } else if (registers[destination_register] >> (16 - 1)) {  /* negative means most significant bit is a 1 */
    registers[R_COND] = FL_NEG;
  } else {
    registers[R_COND] = FL_POS;
  }
}
