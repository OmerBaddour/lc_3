#ifndef MEMORY_H
#define MEMORY_H

#define MEMORY_MAX (1 << 16)

/* memory: declared here, defined once in memory.c */
uint16_t memory[MEMORY_MAX];  /* 2^16 = 65536 locations */

#endif /* MEMORY_H */