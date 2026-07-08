#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disassembler/disassembler.h"
#include "disassembler/operation.h"     /* disassembly_operation_by_code(), the decode table */
#include "core/assembly_directives.h"   /* DIRECTIVE_FILL / _ORIG / _END names */

/* ------------------------------------------------------------------ *
 * turn a line into raw data: `.FILL xNNNN`. The universal fallback for any
 * word that is not a clean instruction encoding — it re-assembles to itself,
 * which is what guarantees the round-trip invariant.
 * ------------------------------------------------------------------ */
static void make_data(DisassemblyInstruction *instruction) {
  instruction->is_data = 1;
  instruction->has_target = 0;
  snprintf(instruction->operation_name, sizeof instruction->operation_name, "%s", DIRECTIVE_FILL.name);
  snprintf(instruction->operands, sizeof instruction->operands, "x%04X", instruction->word);
}

/* ------------------------------------------------------------------ *
 * decode every word, then a second pass to name PC-relative targets
 * ------------------------------------------------------------------ */
DisassemblyInstruction *disassemble(const uint16_t *words, int count, uint16_t origin) {
  DisassemblyInstruction *program = calloc((size_t) (count > 0 ? count : 1), sizeof *program);

  /* PASS 1: decode each word in isolation (mnemonic + operands + maybe target). */
  for (int i = 0; i < count; i++) {
    DisassemblyInstruction *instruction = &program[i];
    instruction->address = (uint16_t) (origin + i);
    instruction->word = words[i];

    const DisassemblyOperation *disassembly_operation =
        disassembly_operation_by_code(words[i] >> 12);
    if (disassembly_operation != NULL && disassembly_operation->decode != NULL) {
      disassembly_operation->decode(words[i], instruction->address, instruction);
    } else {
      instruction->is_data = 1;   /* OP_RES: no decoding */
    }

    if (instruction->is_data) {
      make_data(instruction);     /* format the `.FILL` (mnemonic + operands) once, here */
    }
  }

  /* PASS 2: resolve PC-relative targets to synthetic labels. A target inside
     the program gets a label ("L_3005") minted on the line it points at, then
     copied onto the referencing line. A target OUTSIDE the loaded program can't
     be named (the assembler only takes labels for these operands), so we demote
     that line to `.FILL` — still exact, just less readable. */
  for (int i = 0; i < count; i++) {
    DisassemblyInstruction *instruction = &program[i];
    if (!instruction->has_target) {
      continue;
    }
    uint16_t target = instruction->target;
    if (target >= origin && target < (uint16_t) (origin + count)) {
      DisassemblyInstruction *destination = &program[target - origin];
      if (destination->label[0] == '\0') {
        snprintf(destination->label, sizeof destination->label, "L_%04X", target);
      }
      strcpy(instruction->reference_label, destination->label);
    } else {
      make_data(instruction);
    }
  }

  return program;
}

/* ------------------------------------------------------------------ *
 * rendering
 * ------------------------------------------------------------------ */
void disassembly_instruction_body(const DisassemblyInstruction *instruction, char *out, size_t n) {
  int written = snprintf(out, n, "%s", instruction->operation_name);

  if (instruction->operands[0] != '\0') {
    written += snprintf(out + written, n - (size_t) written, " %s", instruction->operands);
  }
  if (instruction->has_target) {
    /* the label is the first operand for a bare branch/jump, an appended one
       otherwise (e.g. "LEA R0, L_3005"). */
    const char *separator = instruction->operands[0] != '\0' ? ", " : " ";
    snprintf(out + written, n - (size_t) written, "%s%s", separator, instruction->reference_label);
  }
}

void disassembly_render(FILE *out, uint16_t origin,
                        const DisassemblyInstruction *program, int count) {
  fprintf(out, "%s x%04X\n", DIRECTIVE_ORIG.name, origin);
  for (int i = 0; i < count; i++) {
    char body[64];
    disassembly_instruction_body(&program[i], body, sizeof body);
    fprintf(out, "%-8s %s\n", program[i].label, body);
  }
  fprintf(out, "%s\n", DIRECTIVE_END.name);
}
