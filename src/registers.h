#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

/* registers */
enum {
  /* general purpose registers */
  R_R0 = 0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,

  R_PC,     /* program counter */
  R_COND,   /* condition flags */
  R_COUNT   /* sentinel element, trick to get register count */
};

/* condition flags */
enum {
  FL_POS = 1 << 0,  /* positive */
  FL_ZRO = 1 << 1,  /* zero */
  FL_NEG = 1 << 2,  /* negative */
};

/* registers: declared here, defined once in registers.c */
extern uint16_t registers[R_COUNT];

void update_register_condition_flags(
    uint16_t registers[],
    uint16_t destination_register
);

/* memory mapped registers */
enum {
  MR_KBSR = 0xFE00,  /* keyboard status */
  MR_KBDR = 0xFE02  /* keyboard data */
};

#endif /* REGISTERS_H */
