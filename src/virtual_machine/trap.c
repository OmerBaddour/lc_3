#include <stdio.h>
#include <stdint.h>
#include "core/registers.h"
#include "virtual_machine/trap.h"

void trap_getc(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output) {
  (void)memory; (void)output;
  int character = getc(input);
  registers[REGISTER_R0.code] = (uint16_t)character;
  update_register_condition_flags(registers, REGISTER_R0.code);
}

void trap_out(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output) {
  (void)memory; (void)input;
  putc((char)registers[REGISTER_R0.code], output);
  fflush(output);
}

void trap_puts(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output) {
  (void)input;
  /* one character per word */
  uint16_t* character = memory + registers[REGISTER_R0.code];
  while (*character) {
    putc((char)*character, output);
    ++character;
  }
  fflush(output);
}

void trap_in(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output) {
  (void)memory;
  fprintf(output, "Enter character: ");
  int character = getc(input);
  registers[REGISTER_R0.code] = (uint16_t)character;
  putc((char)character, output);
  fflush(output);
}

void trap_putsp(uint16_t memory[], uint16_t registers[], FILE *input, FILE *output) {
  (void)input;
  /* two characters per word: low byte first, then high byte */
  uint16_t* word = memory + registers[REGISTER_R0.code];
  while (*word) {
    char first_character = (*word) & 0xFF;
    putc(first_character, output);
    char second_character = (*word) >> 8;
    if (second_character) {
      putc(second_character, output);
    }
    ++word;
  }
  fflush(output);
}

/* --- the VM subclass table: each entry pairs a shared Trap base with its
   runtime `execute`. HALT has no execute (NULL) — lc_3.c stops the fetch loop
   when it dispatches a trap whose execute is NULL. --- */
static const VirtualMachineTrap VIRTUAL_MACHINE_TRAP_GETC  = { &TRAP_GETC,  trap_getc  };
static const VirtualMachineTrap VIRTUAL_MACHINE_TRAP_OUT   = { &TRAP_OUT,   trap_out   };
static const VirtualMachineTrap VIRTUAL_MACHINE_TRAP_PUTS  = { &TRAP_PUTS,  trap_puts  };
static const VirtualMachineTrap VIRTUAL_MACHINE_TRAP_IN    = { &TRAP_IN,    trap_in    };
static const VirtualMachineTrap VIRTUAL_MACHINE_TRAP_PUTSP = { &TRAP_PUTSP, trap_putsp };
static const VirtualMachineTrap VIRTUAL_MACHINE_TRAP_HALT  = { &TRAP_HALT,  NULL       };

const VirtualMachineTrap *const VIRTUAL_MACHINE_TRAPS[TRAP_COUNT] = {
    &VIRTUAL_MACHINE_TRAP_GETC,  &VIRTUAL_MACHINE_TRAP_OUT,   &VIRTUAL_MACHINE_TRAP_PUTS,
    &VIRTUAL_MACHINE_TRAP_IN,    &VIRTUAL_MACHINE_TRAP_PUTSP, &VIRTUAL_MACHINE_TRAP_HALT,
};

/* Scan by vector, mirroring core's trap_by_code — trap vectors are sparse. */
const VirtualMachineTrap *virtual_machine_trap_by_code(uint16_t code) {
  for (uint16_t i = 0; i < TRAP_COUNT; i++) {
    if (VIRTUAL_MACHINE_TRAPS[i]->trap->code == code) {
      return VIRTUAL_MACHINE_TRAPS[i];
    }
  }
  return NULL;
}
