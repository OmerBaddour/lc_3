#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
/* BEGIN macOS specific stuff for reading from keyboard */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>
/* END macOS specific stuff for reading from keyboard */

#include "registers.h"
#include "memory.h"
#include "trap.h"

/* operations */
enum {
  OP_BR = 0,  /* branch */
  OP_ADD,     /* add */
  OP_LD,      /* load */
  OP_ST,      /* store */
  OP_JSR,     /* jump register */
  OP_AND,     /* bitwise and */
  OP_LDR,     /* load register */
  OP_STR,     /* store register */
  OP_RTI,     /* unused */
  OP_NOT,     /* bitwise not */
  OP_LDI,     /* load indirect */
  OP_STI,     /* store indirect */
  OP_JMP,     /* jump */
  OP_RES,     /* reserved (unused) */
  OP_LEA,     /* load effective address */
  OP_TRAP     /* execute trap */
};

uint16_t sign_extend(uint16_t x, int bit_count) {
  /* sign extend x with bit_count to 16 bits, respecting negatives */
  if ((x >> (bit_count-1)) & 0x1) {
    /* negate by extending with 1s */
    x = x | (0xFFFF << bit_count);
  }
  return x;
}

/* BEGIN macOS specific stuff for reading from keyboard */
struct termios original_tio;

void disable_input_buffering()
{
  tcgetattr(STDIN_FILENO, &original_tio);
  struct termios new_tio = original_tio;
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key()
{
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  return select(1, &readfds, NULL, NULL, &timeout) != 0;
}
/* END macOS specific stuff for reading from keyboard */

uint16_t read_memory(uint16_t address) {
  if (address == MR_KBSR) {
    if (check_key()) {
      memory[MR_KBSR] = (1 << 15);
      memory[MR_KBDR] = getc(stdin);
    } else {
      memory[MR_KBSR] = 0;
    }
  }
  return memory[address];
}

void write_memory(uint16_t address, uint16_t value) {
  memory[address] = value;
}

uint16_t swap16(uint16_t x){
  return (x << 8) | (x >> 8);
}

void read_image_file(FILE* file) {
  /* origin is where in memory to place file */
  uint16_t origin;
  fread(&origin, sizeof(origin), 1, file);
  origin = swap16(origin);

  /* use max file size to inform fread */
  uint16_t max_read = MEMORY_MAX - origin;
  uint16_t *p = memory + origin;
  size_t num_instructions_read = fread(p, sizeof(uint16_t), max_read, file);

  /* swap to little endian */
  while (num_instructions_read-- > 0) {
    *p = swap16(*p);
    ++p;
  }
}

int read_image(const char* image_path){
  FILE* file = fopen(image_path, "rb");
  if (!file) {
    return 0;
  }

  read_image_file(file);
  fclose(file);
  return 1;
}

void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

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
  enum { PC_START = 0x3000 };  /* TODO: figure out why this isn't just `#define`d as a constant */
  registers[R_PC] = PC_START;

  int running = 1;
  while (running) {
    /* FETCH */
    uint16_t instruction = read_memory(registers[R_PC]++);
    uint16_t operation = instruction >> 12;

    switch (operation) {
      case OP_ADD: {
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
        break;
      }
      case OP_AND: {
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
        break;
      }
      case OP_NOT: {
        /*
        15-12 11-9 8-6 5 4-0
        1001   DR  SR  1 1111
        */
        uint16_t destination_register = (instruction >> 9) & 0x7;
        uint16_t source_register = (instruction >> 6) & 0x7;
        uint16_t result = ~registers[source_register];
        registers[destination_register] = result;
        update_register_condition_flags(registers, destination_register);
        break;
      }
      case OP_BR: {
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
        break;
      }
      case OP_JMP: {
        /*
        15-12 11-9 8-6   5-0
        1100  000  BaseR 000000
        */
        /* NOTE: also handles OP_RET since this is where `BaseR = 111` */
        uint16_t base_register = (instruction >> 6) & 0x7;
        registers[R_PC] = registers[base_register];
        break;
      }
      case OP_JSR: {
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
        break;
      }
      case OP_LD: {
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
        break;
      }
      case OP_LDI: {
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
        break;
      }
      case OP_LDR: {
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
        break;
      }
      case OP_LEA: {
        /*
        15-12 11-9 8-0
        1110   DR  PCoffset9
        */
        uint16_t destination_register = (instruction >> 9) & 0x7;
        uint16_t pc_offset_9 = instruction & 0x1FF;
        uint16_t effective_address = registers[R_PC] + sign_extend(pc_offset_9, 9);
        registers[destination_register] = effective_address;
        update_register_condition_flags(registers, destination_register);
        break;
      }
      case OP_ST: {
        /*
        15-12 11-9 8-0
        0011   SR  PCoffset9
        */
        uint16_t source_register = (instruction >> 9) & 0x7;
        uint16_t pc_offset_9 = instruction & 0x1FF;
        uint16_t memory_address = registers[R_PC] + sign_extend(pc_offset_9, 9);
        write_memory(memory_address, registers[source_register]);
        break;
      }
      case OP_STI: {
        /*
        15-12 11-9 8-0
        1011   SR  PCoffset9
        */
        uint16_t source_register = (instruction >> 9) & 0x7;
        uint16_t pc_offset_9 = instruction & 0x1FF;
        uint16_t memory_address_1 = registers[R_PC] + sign_extend(pc_offset_9, 9);
        uint16_t memory_address_2 = read_memory(memory_address_1);
        write_memory(memory_address_2, registers[source_register]);
        break;
      }
      case OP_STR: {
        /*
        15-12 11-9 8-6   5-0
        0111   SR  BaseR offset6
        */
        uint16_t source_register = (instruction >> 9) & 0x7;
        uint16_t base_register = (instruction >> 6) & 0x7;
        uint16_t offset_6 = instruction & 0x3F;
        uint16_t memory_address = registers[base_register] + sign_extend(offset_6, 6);
        write_memory(memory_address, registers[source_register]);
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
            /* one character per word */
            uint16_t* character = memory + registers[R_R0];
            while (*character) {
              putc((char)*character, stdout);
              ++character;
            }
            fflush(stdout);
            break;
          }
          case TRAP_IN: {
            printf("Enter character: ");
            int character = getc(stdin);
            registers[R_R0] = (uint16_t)character;
            putc((char)character, stdout);
            fflush(stdout);
            break;
          }
          case TRAP_PUTSP: {
            /* two characters per word: low byte first, then high byte */
            uint16_t* word = memory + registers[R_R0];
            while (*word) {
              char first_character = (*word) & 0xFF;
              putc(first_character, stdout);
              char second_character = (*word) >> 8;
              if (second_character) {
                putc(second_character, stdout);
              }
              ++word;
            }
            fflush(stdout);
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
