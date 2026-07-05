#include <stdint.h>
#include "core/registers.h"
#include "core/memory.h"
#include "virtual_machine/operation.h"
#include "core/util.h"

void operation_add(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  (void)memory;  /* register-only operation */
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

void operation_and(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  (void)memory;  /* register-only operation */
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

void operation_not(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  (void)memory;  /* register-only operation */
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

void operation_br(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  (void)memory;  /* register-only operation */
  /*
  15-12 11 10 9 8-0
  0000  n  z  p PCoffset9
  */
  uint16_t n = (instruction >> 11) & 0x1;
  uint16_t z = (instruction >> 10) & 0x1;
  uint16_t p = (instruction >> 9) & 0x1;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  if (
    (n & (registers[REGISTER_PROCESSOR_STATUS.code] == CONDITION_FLAG_NEGATIVE))
    | (z & (registers[REGISTER_PROCESSOR_STATUS.code] == CONDITION_FLAG_ZERO))
    | (p & (registers[REGISTER_PROCESSOR_STATUS.code] == CONDITION_FLAG_POSITIVE))
  ) {
    registers[REGISTER_PROGRAM_COUNTER.code] += sign_extend(pc_offset_9, 9);
  }
}

void operation_jmp(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  (void)memory;  /* register-only operation */
  /*
  15-12 11-9 8-6   5-0
  1100  000  BaseR 000000
  */
  /* NOTE: also handles OP_RET since this is where `BaseR = 111` */
  uint16_t base_register = (instruction >> 6) & 0x7;
  registers[REGISTER_PROGRAM_COUNTER.code] = registers[base_register];
}

void operation_jsr(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  (void)memory;  /* register-only operation */
  /*
  15-12 11 10-0
  0100  1  PCoffset11

  or

  15-12 11 10-9 8-6   5-0
  0100  0  00   BaseR 00000
  */
  registers[REGISTER_R7.code] = registers[REGISTER_PROGRAM_COUNTER.code];
  uint16_t is_offset_mode = (instruction >> 11) & 0x1;
  if (is_offset_mode) {
    /* pc_offset_11 */
    uint16_t pc_offset_11 = instruction & 0x7FF;
    registers[REGISTER_PROGRAM_COUNTER.code] += sign_extend(pc_offset_11, 11);
  } else {
    /* base_register */
    uint16_t base_register = (instruction >> 6) & 0x7;
    registers[REGISTER_PROGRAM_COUNTER.code] = registers[base_register];
  }
}

void operation_lea(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  (void)memory;  /* register-only operation */
  /*
  15-12 11-9 8-0
  1110   DR  PCoffset9
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t effective_address = registers[REGISTER_PROGRAM_COUNTER.code] + sign_extend(pc_offset_9, 9);
  registers[destination_register] = effective_address;
  update_register_condition_flags(registers, destination_register);
}

void operation_ld(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  /*
  15-12 11-9 8-0
  0010  DR   PCoffset9
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address = registers[REGISTER_PROGRAM_COUNTER.code] + sign_extend(pc_offset_9, 9);
  uint16_t memory_value = read_memory(memory, memory_address);
  registers[destination_register] = memory_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_ldi(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  /*
  15-12 11-9 8-0
  1010   DR  PCoffset9
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address_1 = registers[REGISTER_PROGRAM_COUNTER.code] + sign_extend(pc_offset_9, 9);
  uint16_t memory_address_2 = read_memory(memory, memory_address_1);
  uint16_t memory_value = read_memory(memory, memory_address_2);
  registers[destination_register] = memory_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_ldr(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  /*
  15-12 11-9 8-6   6-0
  0110   DR  BaseR offset6
  */
  uint16_t destination_register = (instruction >> 9) & 0x7;
  uint16_t base_register = (instruction >> 6) & 0x7;
  uint16_t offset_6 = instruction & 0x3F;
  uint16_t memory_address = registers[base_register] + sign_extend(offset_6, 6);
  uint16_t memory_value = read_memory(memory, memory_address);
  registers[destination_register] = memory_value;
  update_register_condition_flags(registers, destination_register);
}

void operation_st(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  /*
  15-12 11-9 8-0
  0011   SR  PCoffset9
  */
  uint16_t source_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address = registers[REGISTER_PROGRAM_COUNTER.code] + sign_extend(pc_offset_9, 9);
  write_memory(memory, memory_address, registers[source_register]);
}

void operation_sti(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  /*
  15-12 11-9 8-0
  1011   SR  PCoffset9
  */
  uint16_t source_register = (instruction >> 9) & 0x7;
  uint16_t pc_offset_9 = instruction & 0x1FF;
  uint16_t memory_address_1 = registers[REGISTER_PROGRAM_COUNTER.code] + sign_extend(pc_offset_9, 9);
  uint16_t memory_address_2 = read_memory(memory, memory_address_1);
  write_memory(memory, memory_address_2, registers[source_register]);
}

void operation_str(uint16_t instruction, uint16_t registers[], uint16_t memory[]){
  /*
  15-12 11-9 8-6   5-0
  0111   SR  BaseR offset6
  */
  uint16_t source_register = (instruction >> 9) & 0x7;
  uint16_t base_register = (instruction >> 6) & 0x7;
  uint16_t offset_6 = instruction & 0x3F;
  uint16_t memory_address = registers[base_register] + sign_extend(offset_6, 6);
  write_memory(memory, memory_address, registers[source_register]);
}

/* --- the VM subclass table: each entry pairs a shared Operation base with its
   runtime `execute`. Indexed by opcode. Opcodes with no runtime behavior
   (RTI, RES) and TRAP (dispatched specially in lc_3.c, since HALT must stop the
   fetch loop) are left NULL. --- */
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_BR  = { &OPERATION_BR,  operation_br  };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_ADD = { &OPERATION_ADD, operation_add };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_LD  = { &OPERATION_LD,  operation_ld  };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_ST  = { &OPERATION_ST,  operation_st  };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_JSR = { &OPERATION_JSR, operation_jsr };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_AND = { &OPERATION_AND, operation_and };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_LDR = { &OPERATION_LDR, operation_ldr };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_STR = { &OPERATION_STR, operation_str };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_NOT = { &OPERATION_NOT, operation_not };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_LDI = { &OPERATION_LDI, operation_ldi };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_STI = { &OPERATION_STI, operation_sti };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_JMP = { &OPERATION_JMP, operation_jmp };
static const VirtualMachineOperation VIRTUAL_MACHINE_OPERATION_LEA = { &OPERATION_LEA, operation_lea };

const VirtualMachineOperation *const VIRTUAL_MACHINE_OPERATIONS[OPERATION_COUNT] = {
    [OP_BR]  = &VIRTUAL_MACHINE_OPERATION_BR,
    [OP_ADD] = &VIRTUAL_MACHINE_OPERATION_ADD,
    [OP_LD]  = &VIRTUAL_MACHINE_OPERATION_LD,
    [OP_ST]  = &VIRTUAL_MACHINE_OPERATION_ST,
    [OP_JSR] = &VIRTUAL_MACHINE_OPERATION_JSR,
    [OP_AND] = &VIRTUAL_MACHINE_OPERATION_AND,
    [OP_LDR] = &VIRTUAL_MACHINE_OPERATION_LDR,
    [OP_STR] = &VIRTUAL_MACHINE_OPERATION_STR,
    [OP_NOT] = &VIRTUAL_MACHINE_OPERATION_NOT,
    [OP_LDI] = &VIRTUAL_MACHINE_OPERATION_LDI,
    [OP_STI] = &VIRTUAL_MACHINE_OPERATION_STI,
    [OP_JMP] = &VIRTUAL_MACHINE_OPERATION_JMP,
    [OP_LEA] = &VIRTUAL_MACHINE_OPERATION_LEA,
    /* OP_RTI, OP_RES, OP_TRAP intentionally NULL */
};

const VirtualMachineOperation *virtual_machine_operation_by_code(uint16_t code) {
  if (code >= OPERATION_COUNT) {
    return NULL;
  }
  return VIRTUAL_MACHINE_OPERATIONS[code];
}
