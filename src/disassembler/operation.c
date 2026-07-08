#include <stdio.h>
#include <string.h>
#include "disassembler/operation.h"
#include "core/registers.h"   /* register_by_code() */
#include "core/trap.h"        /* trap_by_code() */
#include "core/util.h"        /* sign_extend() */

/* ------------------------------------------------------------------ *
 * decoding helpers (the mirror of assembler/operation.c's encode helpers)
 * ------------------------------------------------------------------ */

/* Register mnemonic for a 3-bit field, e.g. 0 -> "R0". Codes are always 0..7
   here (masked out of the instruction), so the lookup never fails. */
static const char *register_name(uint16_t code) {
  return register_by_code(code)->name;
}

/* Recover a signed immediate from a `bits`-wide two's-complement field. This is
   exactly the inverse of the assembler's immediate_value_to_bits: it masked a
   signed int down to `bits`, we sign-extend those `bits` back up. */
static int signed_field(uint16_t word, int bits) {
  return (int16_t) sign_extend(word & ((1u << bits) - 1), bits);
}

/* Record a PC-relative branch target on `out`. The LC-3 adds the offset to the
   INCREMENTED pc, so the target is (address + 1 + offset) — the inverse of the
   assembler's symbol_table_compute_pc_offset. */
static void set_target(DisassemblyInstruction *out, uint16_t address, int bits) {
  out->has_target = 1;
  out->target = (uint16_t) (address + 1 + signed_field(out->word, bits));
}

/* ------------------------------------------------------------------ *
 * per-operation decoders (one per opcode; the mirror of encode)
 *
 * Each may reject its word (is_data = 1) when the bit pattern is not a valid
 * encoding of this operation — i.e. when re-assembling the decoded mnemonic
 * would NOT reproduce the word. Those words become `.FILL` instead, preserving
 * the disassemble-then-reassemble-is-identity invariant.
 * ------------------------------------------------------------------ */

/* ADD / AND share a shape: reg-reg (bit 5 = 0, bits 4-3 = 0) or reg-imm5. */
static void decode_arithmetic(uint16_t word, DisassemblyInstruction *out) {
  uint16_t dr  = (word >> 9) & 0x7;
  uint16_t sr1 = (word >> 6) & 0x7;
  if ((word >> 5) & 0x1) {
    snprintf(out->operands, sizeof out->operands, "%s, %s, #%d",
             register_name(dr), register_name(sr1), signed_field(word, 5));
  } else if (word & 0x18) {
    out->is_data = 1;   /* bits 4-3 must be 0 in register mode; else not a real ADD/AND */
  } else {
    snprintf(out->operands, sizeof out->operands, "%s, %s, %s",
             register_name(dr), register_name(sr1), register_name(word & 0x7));
  }
}

static void operation_add_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  (void) address;
  strcpy(out->operation_name, OPERATION_ADD.name);
  decode_arithmetic(word, out);
}

static void operation_and_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  (void) address;
  strcpy(out->operation_name, OPERATION_AND.name);
  decode_arithmetic(word, out);
}

static void operation_not_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  (void) address;
  if ((word & 0x3F) != 0x3F) {   /* bits 5-0 must all be 1 for a real NOT */
    out->is_data = 1;
    return;
  }
  strcpy(out->operation_name, OPERATION_NOT.name);
  snprintf(out->operands, sizeof out->operands, "%s, %s",
           register_name((word >> 9) & 0x7), register_name((word >> 6) & 0x7));
}

/* BR[nzp]: condition bits become the mnemonic suffix. nzp = 0 branches on
   nothing — the assembler cannot express it (bare BR means BRnzp), so those
   words are data. */
static void operation_br_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  uint16_t nzp = (word >> 9) & 0x7;
  if (nzp == 0) {
    out->is_data = 1;
    return;
  }
  char *m = out->operation_name;
  *m++ = 'B'; *m++ = 'R';
  if (nzp & 0x4) *m++ = 'n';
  if (nzp & 0x2) *m++ = 'z';
  if (nzp & 0x1) *m++ = 'p';
  *m = '\0';
  set_target(out, address, 9);
}

/* JMP BaseR, or RET when BaseR = R7. The unused fields (bits 11-9, 5-0) must be
   zero, else this is not a real JMP. */
static void operation_jmp_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  (void) address;
  if (word & 0x0E3F) {
    out->is_data = 1;
    return;
  }
  uint16_t base = (word >> 6) & 0x7;
  if (base == 0x7) {
    strcpy(out->operation_name, "RET");   /* RET is an assembler alias (JMP R7), not a core Operation */
  } else {
    strcpy(out->operation_name, OPERATION_JMP.name);
    snprintf(out->operands, sizeof out->operands, "%s", register_name(base));
  }
}

/* JSR label (PC-relative, 11-bit) or JSRR BaseR (register mode). */
static void operation_jsr_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  if ((word >> 11) & 0x1) {
    strcpy(out->operation_name, OPERATION_JSR.name);
    set_target(out, address, 11);
  } else if (word & 0x063F) {   /* bits 10-9 and 5-0 must be zero for JSRR */
    out->is_data = 1;
  } else {
    strcpy(out->operation_name, "JSRR");   /* JSRR is an assembler alias (register-mode JSR), not a core Operation */
    snprintf(out->operands, sizeof out->operands, "%s", register_name((word >> 6) & 0x7));
  }
}

/* PC-relative load/stores: DR/SR then a 9-bit label offset. */
static void decode_pc_relative(uint16_t word, uint16_t address, DisassemblyInstruction *out,
                               const char *operation_name) {
  strcpy(out->operation_name, operation_name);
  snprintf(out->operands, sizeof out->operands, "%s", register_name((word >> 9) & 0x7));
  set_target(out, address, 9);
}

static void operation_ld_decode (uint16_t w, uint16_t a, DisassemblyInstruction *o) { decode_pc_relative(w, a, o, OPERATION_LD.name);  }
static void operation_ldi_decode(uint16_t w, uint16_t a, DisassemblyInstruction *o) { decode_pc_relative(w, a, o, OPERATION_LDI.name); }
static void operation_lea_decode(uint16_t w, uint16_t a, DisassemblyInstruction *o) { decode_pc_relative(w, a, o, OPERATION_LEA.name); }
static void operation_st_decode (uint16_t w, uint16_t a, DisassemblyInstruction *o) { decode_pc_relative(w, a, o, OPERATION_ST.name);  }
static void operation_sti_decode(uint16_t w, uint16_t a, DisassemblyInstruction *o) { decode_pc_relative(w, a, o, OPERATION_STI.name); }

/* base+offset load/stores: DR/SR, BaseR, signed offset6 (all bits significant). */
static void decode_base_offset(uint16_t word, DisassemblyInstruction *out, const char *operation_name) {
  strcpy(out->operation_name, operation_name);
  snprintf(out->operands, sizeof out->operands, "%s, %s, #%d",
           register_name((word >> 9) & 0x7), register_name((word >> 6) & 0x7),
           signed_field(word, 6));
}

static void operation_ldr_decode(uint16_t w, uint16_t a, DisassemblyInstruction *o) { (void) a; decode_base_offset(w, o, OPERATION_LDR.name); }
static void operation_str_decode(uint16_t w, uint16_t a, DisassemblyInstruction *o) { (void) a; decode_base_offset(w, o, OPERATION_STR.name); }

/* TRAP: bits 11-8 must be zero. A known vector prints its alias (PUTS, HALT,
   ...); an unknown one prints `TRAP xNN`. Both re-assemble to the same word. */
static void operation_trap_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  (void) address;
  if (word & 0x0F00) {
    out->is_data = 1;
    return;
  }
  uint16_t vector = word & 0xFF;
  const Trap *trap = trap_by_code(vector);
  if (trap != NULL) {
    snprintf(out->operation_name, sizeof out->operation_name, "%s", trap->name);
  } else {
    strcpy(out->operation_name, OPERATION_TRAP.name);
    snprintf(out->operands, sizeof out->operands, "x%02X", vector);
  }
}

/* RTI carries no operands: everything below the opcode must be zero. */
static void operation_rti_decode(uint16_t word, uint16_t address, DisassemblyInstruction *out) {
  (void) address;
  if (word & 0x0FFF) {
    out->is_data = 1;
    return;
  }
  strcpy(out->operation_name, OPERATION_RTI.name);
}

/* ------------------------------------------------------------------ *
 * the disassembler subclass table + lookup
 * ------------------------------------------------------------------ */
static const DisassemblyOperation DISASSEMBLY_OPERATION_BR   = { &OPERATION_BR,   operation_br_decode   };
static const DisassemblyOperation DISASSEMBLY_OPERATION_ADD  = { &OPERATION_ADD,  operation_add_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_LD   = { &OPERATION_LD,   operation_ld_decode   };
static const DisassemblyOperation DISASSEMBLY_OPERATION_ST   = { &OPERATION_ST,   operation_st_decode   };
static const DisassemblyOperation DISASSEMBLY_OPERATION_JSR  = { &OPERATION_JSR,  operation_jsr_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_AND  = { &OPERATION_AND,  operation_and_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_LDR  = { &OPERATION_LDR,  operation_ldr_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_STR  = { &OPERATION_STR,  operation_str_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_RTI  = { &OPERATION_RTI,  operation_rti_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_NOT  = { &OPERATION_NOT,  operation_not_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_LDI  = { &OPERATION_LDI,  operation_ldi_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_STI  = { &OPERATION_STI,  operation_sti_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_JMP  = { &OPERATION_JMP,  operation_jmp_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_LEA  = { &OPERATION_LEA,  operation_lea_decode  };
static const DisassemblyOperation DISASSEMBLY_OPERATION_TRAP = { &OPERATION_TRAP, operation_trap_decode };

const DisassemblyOperation *const DISASSEMBLY_OPERATIONS[OPERATION_COUNT] = {
    [OP_BR]   = &DISASSEMBLY_OPERATION_BR,
    [OP_ADD]  = &DISASSEMBLY_OPERATION_ADD,
    [OP_LD]   = &DISASSEMBLY_OPERATION_LD,
    [OP_ST]   = &DISASSEMBLY_OPERATION_ST,
    [OP_JSR]  = &DISASSEMBLY_OPERATION_JSR,
    [OP_AND]  = &DISASSEMBLY_OPERATION_AND,
    [OP_LDR]  = &DISASSEMBLY_OPERATION_LDR,
    [OP_STR]  = &DISASSEMBLY_OPERATION_STR,
    [OP_RTI]  = &DISASSEMBLY_OPERATION_RTI,
    [OP_NOT]  = &DISASSEMBLY_OPERATION_NOT,
    [OP_LDI]  = &DISASSEMBLY_OPERATION_LDI,
    [OP_STI]  = &DISASSEMBLY_OPERATION_STI,
    [OP_JMP]  = &DISASSEMBLY_OPERATION_JMP,
    [OP_LEA]  = &DISASSEMBLY_OPERATION_LEA,
    [OP_TRAP] = &DISASSEMBLY_OPERATION_TRAP,
    /* OP_RES has no decoding -> NULL (always rendered as data) */
};

const DisassemblyOperation *disassembly_operation_by_code(uint16_t code) {
  if (code >= OPERATION_COUNT) {
    return NULL;
  }
  return DISASSEMBLY_OPERATIONS[code];
}
