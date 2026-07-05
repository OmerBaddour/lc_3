#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "core/registers.h"
#include "core/memory.h"
#include "virtual_machine/operation.h"
#include "virtual_machine/trap.h"
#include "virtual_machine/io.h"
#include "core/util.h"

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

    if (operation == OP_TRAP) {
      /*
      15-12 11-8 8-0
      1111  0000 trapvect8
      */
      registers[REGISTER_R7.code] = registers[REGISTER_PROGRAM_COUNTER.code];
      uint16_t trapvect_8 = instruction & 0xFF;

      const VirtualMachineTrap *vm_trap = virtual_machine_trap_by_code(trapvect_8);
      if (vm_trap == NULL) {
        /* unknown trap vector */
        abort();
      }
      if (vm_trap->execute == NULL) {
        /* HALT: the one trap that stops the fetch loop (only main can do this) */
        printf("Halting\n");
        running = 0;
      } else {
        vm_trap->execute(memory, registers, stdin, stdout);
      }
    } else {
      /* every other opcode dispatches through the VM operation table */
      const VirtualMachineOperation *vm_operation = virtual_machine_operation_by_code(operation);
      if (vm_operation == NULL || vm_operation->execute == NULL) {
        /* opcodes with no runtime behavior (OP_RES, OP_RTI) */
        abort();
      }
      vm_operation->execute(instruction, registers, memory);
    }
  }

  /* shutdown */
  restore_input_buffering();

  return 0;
}
