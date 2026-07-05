#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "assembler/operation.h"

/* ------------------------------------------------------------------ *
 * encoding helpers (were static in assembler.c; the mirror of the VM's
 * sign_extend lives on the decode side, these on the encode side)
 * ------------------------------------------------------------------ */

static uint16_t operand_register_get_code(const Operand *operand) {
  assert(operand->type == OPERAND_REGISTER);
  return operand->as.reg->code;
}

/* Mask a signed immediate to `bits`, range-checking first. The mask alone IS
   two's-complement encoding (-5 & 0x1F == 0x1B, which sign_extend recovers);
   the check exists because out-of-range values would otherwise wrap silently
   (#16 in imm5 would read back as -16). Sets *ok = 0 on error. */
static uint16_t immediate_value_to_bits(int value, int bits, int *ok) {
  int min = -(1 << (bits - 1));
  int max =  (1 << (bits - 1)) - 1;
  if (value < min || value > max) {
    fprintf(stderr, "error: immediate %d does not fit in a signed %d-bit field [%d, %d]\n",
            value, bits, min, max);
    *ok = 0;
    return 0;
  }
  return (uint16_t) (value & ((1 << bits) - 1));
}

/* Resolve a label to a PC-relative offset, masked to `bits` and range-checked.
   PC-relative because the LC-3 adds the offset to the *incremented* PC, so the
   base is (instr_addr + 1). Sets *ok = 0 on error. */
static uint16_t symbol_table_compute_pc_offset(const SymbolTable *table, const char *label,
                          uint16_t instr_addr, int bits, int *ok) {
  uint16_t target;
  if (!symbol_table_get(table, label, &target)) {
    fprintf(stderr, "error: undefined label '%s'\n", label);
    *ok = 0;
    return 0;
  }
  int offset = (int) target - (int) (instr_addr + 1);
  int min = -(1 << (bits - 1));
  int max =  (1 << (bits - 1)) - 1;
  if (offset < min || offset > max) {
    fprintf(stderr, "error: label '%s' unreachable (offset %d exceeds %d-bit field)\n",
            label, offset, bits);
    *ok = 0;
    return 0;
  }
  return (uint16_t) (offset & ((1 << bits) - 1));
}

/* ------------------------------------------------------------------ *
 * per-operation encoders (one per opcode; the mirror of the VM's execute)
 * ------------------------------------------------------------------ */

/* ADD / AND share a shape: reg-reg or reg-imm5 selected by bit 5. */
static uint16_t encode_arithmetic(const AssemblyInstruction *line, uint16_t opcode, int *ok) {
  const Operand *op = line->operands;
  uint16_t word = (uint16_t) ((opcode << 12)
                              | (operand_register_get_code(&op[0]) << 9)
                              | (operand_register_get_code(&op[1]) << 6));
  if (op[2].type == OPERAND_REGISTER) {
    word |= operand_register_get_code(&op[2]);                                  /* bit 5 = 0, SR2 */
  } else {
    word |= (uint16_t) ((1 << 5) | immediate_value_to_bits(op[2].as.integer, 5, ok));  /* imm5 */
  }
  return word;
}

static uint16_t operation_add_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  (void) table;
  return encode_arithmetic(line, OPERATION_ADD.code, ok);
}

static uint16_t operation_and_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  (void) table;
  return encode_arithmetic(line, OPERATION_AND.code, ok);
}

static uint16_t operation_not_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  (void) table; (void) ok;
  const Operand *op = line->operands;
  return (uint16_t) ((OPERATION_NOT.code << 12) | (operand_register_get_code(&op[0]) << 9)
                     | (operand_register_get_code(&op[1]) << 6) | 0x3F);
}

/* BR[nzp]: the condition bits live in the mnemonic suffix. Bare BR = BRnzp. */
static uint16_t operation_br_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  uint16_t nzp = 0;
  const char *cc = line->operation_name + 2;   /* skip "BR"/"br" (already validated) */
  if (*cc == '\0') {
    nzp = 0x7;
  } else {
    for (; *cc; cc++) {
      switch (toupper((unsigned char) *cc)) {
        case 'N': nzp |= 0x4; break;
        case 'Z': nzp |= 0x2; break;
        case 'P': nzp |= 0x1; break;
      }
    }
  }
  return (uint16_t) ((OPERATION_BR.code << 12) | (nzp << 9)
                     | symbol_table_compute_pc_offset(table, line->operands[0].as.label, line->address, 9, ok));
}

/* JMP BaseR, or RET (= JMP R7, no operand). */
static uint16_t operation_jmp_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  (void) table; (void) ok;
  if (line->operands_length == 0) {
    return (uint16_t) ((OPERATION_JMP.code << 12) | (0x7 << 6));  /* RET */
  }
  return (uint16_t) ((OPERATION_JMP.code << 12) | (operand_register_get_code(&line->operands[0]) << 6));
}

/* JSR label (PC-relative, 11-bit) or JSRR BaseR (register mode). */
static uint16_t operation_jsr_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  const Operand *op = line->operands;
  if (op[0].type == OPERAND_REGISTER) {
    return (uint16_t) ((OPERATION_JSR.code << 12) | (operand_register_get_code(&op[0]) << 6));    /* JSRR */
  }
  return (uint16_t) ((OPERATION_JSR.code << 12) | (1 << 11)
                     | symbol_table_compute_pc_offset(table, op[0].as.label, line->address, 11, ok));
}

/* PC-relative load/stores: opcode | DR/SR | PCoffset9 (label in operand 1). */
static uint16_t encode_pc_relative(const AssemblyInstruction *line, uint16_t opcode,
                                   const SymbolTable *table, int *ok) {
  const Operand *op = line->operands;
  return (uint16_t) ((opcode << 12) | (operand_register_get_code(&op[0]) << 9)
                     | symbol_table_compute_pc_offset(table, op[1].as.label, line->address, 9, ok));
}

static uint16_t operation_ld_encode (const AssemblyInstruction *l, const SymbolTable *t, int *ok) { return encode_pc_relative(l, OPERATION_LD.code,  t, ok); }
static uint16_t operation_ldi_encode(const AssemblyInstruction *l, const SymbolTable *t, int *ok) { return encode_pc_relative(l, OPERATION_LDI.code, t, ok); }
static uint16_t operation_lea_encode(const AssemblyInstruction *l, const SymbolTable *t, int *ok) { return encode_pc_relative(l, OPERATION_LEA.code, t, ok); }
static uint16_t operation_st_encode (const AssemblyInstruction *l, const SymbolTable *t, int *ok) { return encode_pc_relative(l, OPERATION_ST.code,  t, ok); }
static uint16_t operation_sti_encode(const AssemblyInstruction *l, const SymbolTable *t, int *ok) { return encode_pc_relative(l, OPERATION_STI.code, t, ok); }

/* base+offset load/stores: opcode | DR/SR | BaseR | offset6. */
static uint16_t encode_base_offset(const AssemblyInstruction *line, uint16_t opcode, int *ok) {
  const Operand *op = line->operands;
  return (uint16_t) ((opcode << 12) | (operand_register_get_code(&op[0]) << 9)
                     | (operand_register_get_code(&op[1]) << 6)
                     | immediate_value_to_bits(op[2].as.integer, 6, ok));
}

static uint16_t operation_ldr_encode(const AssemblyInstruction *l, const SymbolTable *t, int *ok) { (void) t; return encode_base_offset(l, OPERATION_LDR.code, ok); }
static uint16_t operation_str_encode(const AssemblyInstruction *l, const SymbolTable *t, int *ok) { (void) t; return encode_base_offset(l, OPERATION_STR.code, ok); }

static uint16_t operation_trap_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  (void) table;
  /* trapvect8 is UNSIGNED (a vector-table index), unlike imm5/offset6. */
  int vector = line->operands[0].as.integer;
  if (vector < 0 || vector > 0xFF) {
    fprintf(stderr, "error: trap vector %d does not fit in 8 bits [0, 255]\n", vector);
    *ok = 0;
    return 0;
  }
  return (uint16_t) ((OPERATION_TRAP.code << 12) | vector);
}

static uint16_t operation_rti_encode(const AssemblyInstruction *line, const SymbolTable *table, int *ok) {
  (void) line; (void) table; (void) ok;
  return (uint16_t) (OPERATION_RTI.code << 12);
}

/* ------------------------------------------------------------------ *
 * the assembler subclass table + family-aware lookup
 * ------------------------------------------------------------------ */
static const AssemblyOperation ASSEMBLY_OPERATION_BR   = { &OPERATION_BR,   operation_br_encode   };
static const AssemblyOperation ASSEMBLY_OPERATION_ADD  = { &OPERATION_ADD,  operation_add_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_LD   = { &OPERATION_LD,   operation_ld_encode   };
static const AssemblyOperation ASSEMBLY_OPERATION_ST   = { &OPERATION_ST,   operation_st_encode   };
static const AssemblyOperation ASSEMBLY_OPERATION_JSR  = { &OPERATION_JSR,  operation_jsr_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_AND  = { &OPERATION_AND,  operation_and_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_LDR  = { &OPERATION_LDR,  operation_ldr_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_STR  = { &OPERATION_STR,  operation_str_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_RTI  = { &OPERATION_RTI,  operation_rti_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_NOT  = { &OPERATION_NOT,  operation_not_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_LDI  = { &OPERATION_LDI,  operation_ldi_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_STI  = { &OPERATION_STI,  operation_sti_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_JMP  = { &OPERATION_JMP,  operation_jmp_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_LEA  = { &OPERATION_LEA,  operation_lea_encode  };
static const AssemblyOperation ASSEMBLY_OPERATION_TRAP = { &OPERATION_TRAP, operation_trap_encode };

const AssemblyOperation *const ASSEMBLY_OPERATIONS[OPERATION_COUNT] = {
    [OP_BR]   = &ASSEMBLY_OPERATION_BR,
    [OP_ADD]  = &ASSEMBLY_OPERATION_ADD,
    [OP_LD]   = &ASSEMBLY_OPERATION_LD,
    [OP_ST]   = &ASSEMBLY_OPERATION_ST,
    [OP_JSR]  = &ASSEMBLY_OPERATION_JSR,
    [OP_AND]  = &ASSEMBLY_OPERATION_AND,
    [OP_LDR]  = &ASSEMBLY_OPERATION_LDR,
    [OP_STR]  = &ASSEMBLY_OPERATION_STR,
    [OP_RTI]  = &ASSEMBLY_OPERATION_RTI,
    [OP_NOT]  = &ASSEMBLY_OPERATION_NOT,
    [OP_LDI]  = &ASSEMBLY_OPERATION_LDI,
    [OP_STI]  = &ASSEMBLY_OPERATION_STI,
    [OP_JMP]  = &ASSEMBLY_OPERATION_JMP,
    [OP_LEA]  = &ASSEMBLY_OPERATION_LEA,
    [OP_TRAP] = &ASSEMBLY_OPERATION_TRAP,
    /* OP_RES has no encoding -> NULL */
};

const AssemblyOperation *assembly_operation_by_name(const char *upper_name) {
  /* BR family: "BR" followed by only condition-code letters (possibly none). */
  if (upper_name[0] == 'B' && upper_name[1] == 'R') {
    int is_branch = 1;
    for (size_t i = 2; upper_name[i] != '\0'; i++) {
      if (upper_name[i] != 'N' && upper_name[i] != 'Z' && upper_name[i] != 'P') {
        is_branch = 0;
        break;
      }
    }
    if (is_branch) {
      return ASSEMBLY_OPERATIONS[OP_BR];
    }
  }
  if (strcmp(upper_name, "RET")  == 0) return ASSEMBLY_OPERATIONS[OP_JMP];  /* RET = JMP R7 */
  if (strcmp(upper_name, "JSRR") == 0) return ASSEMBLY_OPERATIONS[OP_JSR];  /* register-mode JSR */

  const Operation *base = operation_by_name(upper_name);
  if (base != NULL) {
    return ASSEMBLY_OPERATIONS[base->code];   /* NULL for OP_RES */
  }
  return NULL;
}
