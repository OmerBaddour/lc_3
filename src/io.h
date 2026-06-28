#ifndef IO_H
#define IO_H

#include <stdint.h>

/* macOS specific terminal / keyboard handling */
void disable_input_buffering(void);
void restore_input_buffering(void);
uint16_t check_key(void);

#endif /* IO_H */
