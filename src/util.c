#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "registers.h"
#include "memory.h"
#include "io.h"

/*
LC_3 helpers
*/

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

/*
StringList helpers
*/

/* trim slice [start, end) into new heap string */
static char *trim_slice(const char *start, const char *end) {
  while (start < end && isspace((unsigned char) start[0])) {
    start++;
  }
  while (end > start && isspace((unsigned char) end[-1])) {
    end--;
  }
  size_t length = (size_t)(end - start);
  char *output = malloc(length + 1);
  memcpy(output, start, length);
  output[length] = '\0';
  return output;
}

char *trim(const char *string) {
  return trim_slice(string, string + strlen(string));
}

static void append(StringList *list, char *string) {
  list->data = realloc(list->data, (size_t)(list->length + 1) * sizeof *list->data);
  list->data[list->length] = string;
  list->length++;
}

StringList *split(const char *string, char delimiter) {
  StringList *list = calloc(1, sizeof(StringList));

  char *start = (char *) string;
  char *end = (char *) string;
  for (;;end++) {
    if (*end == delimiter || *end == '\0') {
      append(list, trim_slice(start, end));
      start = end + 1;
      if (*end == '\0') {
        break;
      }
    }
  }
  return list;
}

void free_string_list(StringList *list) {
    for (uint16_t string_index = 0; string_index < list->length; string_index++) {
      free(list->data[string_index]);
    }
    free(list->data);
    free(list);
}
