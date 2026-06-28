#include <stdint.h>
#include "registers.h"
#include "memory.h"
#include "operations.h"

uint16_t sign_extend(uint16_t x, int bit_count) {
  /* sign extend x with bit_count to 16 bits, respecting negatives */
  if ((x >> (bit_count-1)) & 0x1) {
    /* negate by extending with 1s */
    x = x | (0xFFFF << bit_count);
  }
  return x;
}

void operation_add(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-6 5 4-3 2-0
  0001   DR  SR1 0 00  SR2

  or

  15-12 11-9 8-6 5 4-0
  0001   DR  SR1 1 imm5
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t source_register_1 = (instruction >> 6) & 0x7;
  uint16_t is_immediate_mode = (instruction >> 5) & 0x1;
  uint16_t other_value = 0;
  if (is_immediate_mode) {
    /* imm_5 */
    uint16_t imm_5 = instruction & 0x1F;
    other_value = sign_extend(imm_5, 5);
  } else {
    /* SR2 */
    uint16_t source_register_2 = instruction & 0x7;
    other_value = registers[source_register_2];
  }
  registers[destination_register] = registers[source_register_1] + other_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_and(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-6 5 4-3 2-0
  0101   DR  SR1 0 00  SR2

  or

  15-12 11-9 8-6 5 4-0
  0101   DR  SR1 1 imm5
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t source_register_1 = (instruction >> 6) & 0x7;
  uint16_t is_immediate_mode = (instruction >> 5) & 0x1;
  uint16_t other_value = 0;
  if (is_immediate_mode) {
    /* imm_5 */
    uint16_t imm_5 = instruction & 0x1F;
    other_value = sign_extend(imm_5, 5);
  } else {
    /* SR2 */
    uint16_t source_register_2 = instruction & 0x7;
    other_value = registers[source_register_2];
  }
  registers[destination_register] = registers[source_register_1] & other_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_not(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-6 5 4-0
  1001   DR  SR  1 1111
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t source_register = (instruction >> 6) & 0x7;
  uint16_t result = ~registers[source_register];
  registers[destination_register] = result;
  update_register_condition_flags(registers, destination_register);
}

void operation_br(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11 10 9 8-0
  0000  n  z  p PCoffset9
  */
  uint16_t n = (instruction >> 11) & 0x1;
  uint16_t z = (instruction >> 10) & 0x1;
  uint16_t p = (instruction >> 9) & 0x1;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  if (
    (n & (registers[R_COND] == FL_NEG))
    | (z & (registers[R_COND] == FL_ZRO))
    | (p & (registers[R_COND] == FL_POS))
  ) {
    registers[R_PC] += sign_extend(pc_offset_9, 9);
  }
}

void operation_jmp(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-6   5-0
  1100  000  BaseR 000000
  */
  /* NOTE: also handles OP_RET since this is where `BaseR = 111` */
  uint16_t base_register = (instruction >> 6) & 0x7;
  registers[R_PC] = registers[base_register];
}

void operation_jsr(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11 10-0
  0100  1  PCoffset11

  or

  15-12 11 10-9 8-6   5-0
  0100  0  00   BaseR 00000
  */
  registers[R_R7] = registers[R_PC];
  uint16_t is_offset_mode = (instruction >> 11) & 0x1;
  if (is_offset_mode) {
    /* pc_offset_11 */
    uint16_t pc_offset_11 = instruction & 0x7FF;
    registers[R_PC] += sign_extend(pc_offset_11, 11);
  } else {
    /* base_register */
    uint16_t base_register = (instruction >> 6) & 0x7;
    registers[R_PC] = registers[base_register];
  }
}

void operation_lea(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-0
  1110   DR  PCoffset9
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t effective_address = registers[R_PC] + sign_extend(pc_offset_9, 9);
  registers[destination_register] = effective_address;
  update_register_condition_flags(registers, destination_register);
}

void operation_ld(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-0
  0010  DR   PCoffset9
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address = registers[R_PC] + sign_extend(pc_offset_9, 9);
  uint16_t memory_value = read_memory(memory_address);
  registers[destination_register] = memory_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_ldi(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-0
  1010   DR  PCoffset9
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address_1 = registers[R_PC] + sign_extend(pc_offset_9, 9);
  uint16_t memory_address_2 = read_memory(memory_address_1);
  uint16_t memory_value = read_memory(memory_address_2);
  registers[destination_register] = memory_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_ldr(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-6   6-0
  0110   DR  BaseR offset6
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t base_register = (instruction >> 6) & 0x7;
  uint16_t offset_6 = instruction & 0x3F;
  uint16_t memory_address = registers[base_register] + sign_extend(offset_6, 6);
  uint16_t memory_value = read_memory(memory_address);
  registers[destination_register] = memory_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_st(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-0
  0011   SR  PCoffset9
  */
  uint16_t source_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address = registers[R_PC] + sign_extend(pc_offset_9, 9);
  write_memory(memory_address, registers[source_register]);
}

void operation_sti(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-0
  1011   SR  PCoffset9
  */
  uint16_t source_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address_1 = registers[R_PC] + sign_extend(pc_offset_9, 9);
  uint16_t memory_address_2 = read_memory(memory_address_1);
  write_memory(memory_address_2, registers[source_register]);
}

void operation_str(uint16_t instruction, uint16_t registers[]){
  /*
  15-12 11-9 8-6   5-0
  0111   SR  BaseR offset6
  */
  uint16_t source_register = (instruction >> 9) & 0x7;
  uint16_t base_register = (instruction >> 6) & 0x7;
  uint16_t offset_6 = instruction & 0x3F;
  uint16_t memory_address = registers[base_register] + sign_extend(offset_6, 6);
  write_memory(memory_address, registers[source_register]);
}
