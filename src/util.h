#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdio.h>

uint16_t swap16(uint16_t x);
uint16_t read_image_file(FILE* file);
uint16_t read_image(const char* image_path);
void handle_interrupt(int signal);

#endif /* UTIL_H */
