//EagleOS 1.0 Keyboard Library.
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KEY_NONE   0
#define KEY_UP     1
#define KEY_DOWN   2
#define KEY_LEFT   3
#define KEY_RIGHT  4
#define KEY_ENTER  5
#define KEY_ESCAPE 6
#define KEY_TAB    7
#define KEY_BACKSPACE 8

#define KEY_CHAR_BASE 256u
#define KEY_CHAR(ch) ((uint16_t)(KEY_CHAR_BASE + (uint8_t)(ch)))
#define KEY_IS_CHAR(key) ((uint16_t)(key) >= KEY_CHAR_BASE)
#define KEY_TO_CHAR(key) ((char)((uint16_t)(key) - KEY_CHAR_BASE))

uint16_t keyboard_poll_key(void);

#endif // KEYBOARD_H
