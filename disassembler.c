#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disassembler/disassembler.h"

/* ------------------------------------------------------------------ *
 * driver: read object file -> decode -> write assembly listing
 * All the real work lives in src/disassembler/ so the test binaries can
 * link it; this file is only main().
 * ------------------------------------------------------------------ */

/* Read an LC-3 object file: the first big-endian word is the origin, the rest
   are program words. Mirrors write_object_file on the assembler side (and the
   VM loader's byte order). Returns a heap array of `*count_out` words, or NULL
   on error. Caller frees. */
static uint16_t *read_object_file(const char *path, int *count_out, uint16_t *origin_out) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "could not open %s\n", path);
    return NULL;
  }

  int hi = fgetc(file), lo = fgetc(file);
  if (hi == EOF || lo == EOF) {
    fprintf(stderr, "%s is too short to hold an origin word\n", path);
    fclose(file);
    return NULL;
  }
  *origin_out = (uint16_t) ((hi << 8) | lo);

  uint16_t *words = NULL;
  int count = 0, capacity = 0;
  while ((hi = fgetc(file)) != EOF && (lo = fgetc(file)) != EOF) {
    if (count == capacity) {
      capacity = capacity ? capacity * 2 : 16;
      words = realloc(words, (size_t) capacity * sizeof *words);
    }
    words[count++] = (uint16_t) ((hi << 8) | lo);
  }
  fclose(file);

  *count_out = count;
  return words;
}

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s <input.obj> [output.asm]\n", argv[0]);
    return 2;
  }

  int count = 0;
  uint16_t origin = 0;
  uint16_t *words = read_object_file(argv[1], &count, &origin);
  if (words == NULL) {
    return 1;
  }

  DisassemblyInstruction *program = disassemble(words, count, origin);

  FILE *out = stdout;
  if (argc == 3) {
    out = fopen(argv[2], "w");
    if (!out) {
      fprintf(stderr, "could not open %s for writing\n", argv[2]);
      free(program);
      free(words);
      return 1;
    }
  }

  disassembly_render(out, origin, program, count);

  if (out != stdout) {
    fclose(out);
    printf("wrote %s\n", argv[2]);
  }

  free(program);
  free(words);
  return 0;
}
