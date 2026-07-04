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
  /* load the single image argument */
  if (argc != 2) {
    /* show usage string */
    printf("lc_3 [image_file]\n");
    exit(2);
  }
  /* execution begins at the image's origin; a valid origin is always
     >= USER_SPACE_START, so 0 unambiguously signals a load failure */
  uint16_t origin = read_image(argv[1]);
  if (!origin) {
    printf("failed to load image: %s\n", argv[1]);
    exit(1);
  }

  /* setup */
  signal(SIGINT, handle_interrupt);
  disable_input_buffering();
  uint16_t registers[REGISTER_COUNT] = {0};

  /* exactly one condition flag should be set at all times */
  /* set zero condition flag arbitrarily */
  registers[REGISTER_PROCESSOR_STATUS.code] = CONDITION_FLAG_ZERO;

  /* set program counter to where the image was actually loaded */
  registers[REGISTER_PROGRAM_COUNTER.code] = origin;

  int running = 1;
  while (running) {
    /* FETCH */
    uint16_t instruction = read_memory(memory, registers[REGISTER_PROGRAM_COUNTER.code]++);
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
        registers[REGISTER_R7.code] = registers[REGISTER_PROGRAM_COUNTER.code];
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
