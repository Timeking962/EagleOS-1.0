// EagleOS 1.0 Mouse Library.
#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void mouse_init(void);
uint8_t mouse_poll(int8_t *dx, int8_t *dy, uint8_t *left_button, uint8_t *right_button);

#endif // MOUSE_H
