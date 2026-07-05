#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define MEMORY_MAX (1 << 16)

/* memory: declared here, defined once in memory.c */
extern uint16_t memory[MEMORY_MAX];  /* 2^16 = 65536 locations */

uint16_t read_memory(uint16_t memory[], uint16_t address);
void write_memory(uint16_t memory[], uint16_t address, uint16_t value);

#endif /* MEMORY_H */