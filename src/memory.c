#include <stdint.h>
#include <stdio.h>
#include "memory.h"
#include "registers.h"
#include "io.h"

/* the one real definition of the memory file */
uint16_t memory[MEMORY_MAX];  /* 2^16 = 65536 locations */

uint16_t read_memory(uint16_t memory[], uint16_t address) {
  if (address == MR_KBSR) {
    if (check_key()) {
      memory[MR_KBSR] = (1 << 15);
      memory[MR_KBDR] = getc(stdin);
    } else {
      memory[MR_KBSR] = 0;
    }
  }
  return memory[address];
}

void write_memory(uint16_t memory[], uint16_t address, uint16_t value) {
  memory[address] = value;
}
