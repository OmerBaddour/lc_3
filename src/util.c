#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "registers.h"
#include "memory.h"
#include "io.h"

uint16_t swap16(uint16_t x){
  return (x << 8) | (x >> 8);
}

uint16_t read_image_file(FILE* file) {
  /* origin is where in memory to place file */
  uint16_t origin;
  fread(&origin, sizeof(origin), 1, file);
  origin = swap16(origin);

  /* sanity check the origin against malformed obj files: user programs must
     start at or above USER_SPACE_START, since below is reserved by convention
     for the trap/interrupt vector tables and OS space */
  if (origin < USER_SPACE_START) {
    fprintf(
        stderr,
        "invalid origin 0x%04x: below user space (0x%04x)\n",
        origin,
        USER_SPACE_START
    );
    exit(1);
  }

  /* use max file size to inform fread */
  uint16_t max_read = MEMORY_MAX - origin;
  uint16_t *p = memory + origin;
  size_t num_instructions_read = fread(p, sizeof(uint16_t), max_read, file);

  /* swap to little endian */
  while (num_instructions_read-- > 0) {
    *p = swap16(*p);
    ++p;
  }

  return origin;
}

uint16_t read_image(const char* image_path){
  FILE* file = fopen(image_path, "rb");
  if (!file) {
    return 0;
  }

  uint16_t origin = read_image_file(file);
  fclose(file);
  return origin;
}

void handle_interrupt(int signal)
{
    (void)signal;
    restore_input_buffering();
    printf("\n");
    exit(-2);
}
