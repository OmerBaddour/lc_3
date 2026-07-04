#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

#define REGISTER_COUNT 10

typedef struct Register {
    const uint16_t code;
    const char *name;
} Register;

extern const Register REGISTER_R0;
extern const Register REGISTER_R1;
extern const Register REGISTER_R2;
extern const Register REGISTER_R3;
extern const Register REGISTER_R4;
extern const Register REGISTER_R5;
extern const Register REGISTER_R6;
extern const Register REGISTER_R7;
extern const Register REGISTER_PROGRAM_COUNTER;
extern const Register REGISTER_PROCESSOR_STATUS;

extern const Register *const REGISTERS[REGISTER_COUNT];

const Register *register_by_code(uint16_t code);
const Register *register_by_name(const char *name);

/* condition flags */
enum {
  CONDITION_FLAG_POSITIVE = 1 << 0,
  CONDITION_FLAG_ZERO     = 1 << 1,
  CONDITION_FLAG_NEGATIVE = 1 << 2,
};

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
