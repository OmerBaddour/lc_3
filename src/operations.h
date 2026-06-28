#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stdint.h>

uint16_t sign_extend(uint16_t x, int bit_count);

/* register-only operations */
void operation_add(uint16_t instruction, uint16_t registers[]);
void operation_and(uint16_t instruction, uint16_t registers[]);
void operation_not(uint16_t instruction, uint16_t registers[]);
void operation_br(uint16_t instruction, uint16_t registers[]);
void operation_jmp(uint16_t instruction, uint16_t registers[]);
void operation_jsr(uint16_t instruction, uint16_t registers[]);
void operation_lea(uint16_t instruction, uint16_t registers[]);

/* memory-reading operations */
void operation_ld(uint16_t instruction, uint16_t registers[]);
void operation_ldi(uint16_t instruction, uint16_t registers[]);
void operation_ldr(uint16_t instruction, uint16_t registers[]);

/* memory-writing operations */
void operation_st(uint16_t instruction, uint16_t registers[]);
void operation_sti(uint16_t instruction, uint16_t registers[]);
void operation_str(uint16_t instruction, uint16_t registers[]);

#endif /* OPERATIONS_H */
