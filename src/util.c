#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "memory.h"
#include "io.h"

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
    (void)signal;
    restore_input_buffering();
    printf("\n");
    exit(-2);
}
