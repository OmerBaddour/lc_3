#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
  The disassembler is the mirror image of the assembler: the assembler turns
  text into 16-bit words, the disassembler turns 16-bit words back into text.
  Its "intermediary" — the counterpart of AssemblyInstruction — is one decoded
  program word.

  The hard truth of disassembly is that an object file is just a flat run of
  words with NO markers separating code from data: the word 0x0048 is both the
  letter 'H' and a (degenerate) BR instruction. We resolve this with a single
  invariant that every decode obeys:

      disassemble(word) then re-assemble  ==  word,  for EVERY 16-bit word.

  A word is decoded to a real instruction ONLY when that instruction re-encodes
  back to the exact same bits; otherwise it is emitted as `.FILL xNNNN`, which
  trivially round-trips. So label names and the original code/data split are
  lost (a disassembler cannot recover them), but the machine code is preserved
  perfectly — which is the honest definition of a correct disassembler.

  Unlike Operand in the assembler, the fields here are small fixed buffers, not
  heap-owned strings: a decoded line never grows, so inline storage keeps the
  struct copyable and leak-free (there is nothing to free).
*/
typedef struct DisassemblyInstruction {
  uint16_t address;       /* where this word lives (origin + its index) */
  uint16_t word;          /* the raw 16-bit program word being decoded */

  int is_data;            /* 1 => render as `.FILL xNNNN` (see invariant above) */

  char label[16];         /* label DEFINED at this address ("L_3005"), or "" if none.
                             Assigned in the labeling pass when some other line
                             targets this address. */
  char operation_name[8]; /* the decoded op/directive: "ADD", "BRnzp", "PUTS",
                             "TRAP", ".FILL" ... — the counterpart of
                             AssemblyInstruction.operation_name. */
  char operands[32];      /* the fixed operand text, e.g. "R0, R1, R2" or "R0" or ""
                             — everything EXCEPT a PC-relative label operand. */

  int has_target;         /* 1 => the final operand is a PC-relative label */
  uint16_t target;        /* absolute address that label refers to */
  char reference_label[16]; /* the target line's label, copied in during labeling */
} DisassemblyInstruction;

/* Decode `count` program words (loaded at `origin`, NOT counting the origin
   header word) into a freshly allocated array of decoded lines. Runs the
   labeling pass so every in-range PC-relative target gets a synthetic label.
   Caller frees the returned array with a single free(). */
DisassemblyInstruction *disassemble(const uint16_t *words, int count, uint16_t origin);

/* Compose one decoded line's text (mnemonic + operands + any label operand),
   WITHOUT its own leading label. Writes at most `n` bytes including the NUL. */
void disassembly_instruction_body(const DisassemblyInstruction *instruction, char *out, size_t n);

/* Print a full reassemblable program: `.ORIG`, one line per decoded word (with
   its label in a leading column), then `.END`. */
void disassembly_render(FILE *out, uint16_t origin,
                        const DisassemblyInstruction *program, int count);

#endif /* DISASSEMBLER_H */
