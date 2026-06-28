#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "registers.h"
#include "memory.h"
#include "operations.h"
#include "trap.h"
#include "io.h"
#include "util.h"

int main(int argc, const char *argv[]) {
  /* load arguments */
  if (argc < 2) {
    /* show usage string */
    printf("lc3 [image-file-1] ...\n");
    exit(2);
  }
  for (int j = 1; j < argc; ++j) {
    if (!read_image(argv[j])) {
      printf("failed to load image: %s\n", argv[j]);
      exit(1);
    }
  }

  /* setup */
  signal(SIGINT, handle_interrupt);
  disable_input_buffering();

  /* exactly one condition flag should be set at all times */
  /* set zero condition flag arbitrarily */
  registers[R_COND] = FL_ZRO;

  /* set program counter */
  registers[R_PC] = PC_START;

  int running = 1;
  while (running) {
    /* FETCH */
    uint16_t instruction = read_memory(memory, registers[R_PC]++);
    uint16_t operation = instruction >> 12;

    switch (operation) {
      case OP_ADD: {
        operation_add(instruction, registers);
        break;
      }
      case OP_AND: {
        operation_and(instruction, registers);
        break;
      }
      case OP_NOT: {
        operation_not(instruction, registers);
        break;
      }
      case OP_BR: {
        operation_br(instruction, registers);
        break;
      }
      case OP_JMP: {
        operation_jmp(instruction, registers);
        break;
      }
      case OP_JSR: {
        operation_jsr(instruction, registers);
        break;
      }
      case OP_LD: {
        operation_ld(instruction, registers, memory);
        break;
      }
      case OP_LDI: {
        operation_ldi(instruction, registers, memory);
        break;
      }
      case OP_LDR: {
        operation_ldr(instruction, registers, memory);
        break;
      }
      case OP_LEA: {
        operation_lea(instruction, registers);
        break;
      }
      case OP_ST: {
        operation_st(instruction, registers, memory);
        break;
      }
      case OP_STI: {
        operation_sti(instruction, registers, memory);
        break;
      }
      case OP_STR: {
        operation_str(instruction, registers, memory);
        break;
      }
      case OP_TRAP: {
        /*
        15-12 11-8 8-0
        1111  0000 trapvect8
        */
        registers[R_R7] = registers[R_PC];
        uint16_t trapvect_8 = instruction & 0xFF;

        switch (trapvect_8) {
          case TRAP_GETC: {
            trap_getc(registers, stdin);
            break;
          }
          case TRAP_OUT: {
            trap_out(registers, stdout);
            break;
          }
          case TRAP_PUTS: {
            trap_puts(memory, registers, stdout);
            break;
          }
          case TRAP_IN: {
            trap_in(registers, stdin, stdout);
            break;
          }
          case TRAP_PUTSP: {
            trap_putsp(memory, registers, stdout);
            break;
          }
          case TRAP_HALT: {
            printf("Halting\n");
            running = 0;
            break;
          }
        }
        break;
      }
      case OP_RES:
      case OP_RTI:
      default:
        /* bad operation */
        abort();
    }
  }

  /* shutdown */
  restore_input_buffering();

  return 0;
}
