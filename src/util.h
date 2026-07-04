#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>

/* lc_3 */
enum { USER_SPACE_START = 0x3000 };  /* address the program counter starts at by convention */
uint16_t swap16(uint16_t x);
uint16_t read_image_file(FILE* file);
uint16_t read_image(const char* image_path);
void handle_interrupt(int signal);

/* StringList */
typedef struct {
    char **data;
    int length;
} StringList;
char *trim(const char *string);
StringList *split(const char *string, char delimiter);
void free_string_list(StringList *string_list);


#endif /* UTIL_H */
