#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assembler.h"

/* ------------------------------------------------------------------ *
 * driver: parse -> assemble -> write object file (+ a listing)
 * All the real work lives in src/assembler.c so the test binaries can
 * link it; this file is only main().
 * ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage: %s <input.asm> [output.obj]\n", argv[0]);
    return 2;
  }

  FILE *input = fopen(argv[1], "r");
  if (!input) {
    fprintf(stderr, "could not open %s\n", argv[1]);
    return 1;
  }

  /* Collect every non-blank line into a growable array of intermediaries. */
  AssemblyInstruction **program = NULL;
  int count = 0, capacity = 0;
  char line[256];
  while (fgets(line, sizeof line, input)) {
    AssemblyInstruction *instruction = parse_assembly_line(line);
    if (instruction == NULL) {
      continue;  /* blank or comment-only line */
    }
    if (count == capacity) {
      capacity = capacity ? capacity * 2 : 16;
      program = realloc(program, (size_t) capacity * sizeof *program);
    }
    program[count++] = instruction;
  }
  fclose(input);

  uint16_t origin = 0;
  int ok = assemble(program, count, &origin);

  if (ok) {
    /* Derive output path: foo.asm -> foo.obj (or append .obj), unless given. */
    char out_path[300];
    if (argc == 3) {
      snprintf(out_path, sizeof out_path, "%s", argv[2]);
    } else {
      size_t n = strlen(argv[1]);
      if (n > 4 && strcmp(argv[1] + n - 4, ".asm") == 0) {
        snprintf(out_path, sizeof out_path, "%.*s.obj", (int) (n - 4), argv[1]);
      } else {
        snprintf(out_path, sizeof out_path, "%s.obj", argv[1]);
      }
    }

    /* A little assembler listing: address, mnemonic, and the emitted words. */
    printf("origin  0x%04X\n", origin);
    for (int i = 0; i < count; i++) {
      AssemblyInstruction *l = program[i];
      printf("0x%04X  %-10s", l->address, l->operation_name ? l->operation_name : "");
      for (int j = 0; j < l->machine_codes_length; j++) {
        printf(" %04X", l->machine_codes[j]);
      }
      printf("\n");
    }

    if (write_object_file(out_path, origin, program, count)) {
      printf("wrote %s\n", out_path);
    } else {
      ok = 0;
    }
  }

  for (int i = 0; i < count; i++) {
    free_assembly_instruction(program[i]);
  }
  free(program);
  return ok ? 0 : 1;
}
