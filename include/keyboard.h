// EagleOS 1.0 Keyboard Library.
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/*
 * Special keys.
 *
 * Character keys are returned as:
 *
 *     KEY_CHAR_BASE + ASCII
 *
 * Example:
 *
 *     'a' -> KEY_CHAR('a')
 *     'A' -> KEY_CHAR('A')
 */
#define KEY_NONE       0

#define KEY_UP         1
#define KEY_DOWN       2
#define KEY_LEFT       3
#define KEY_RIGHT      4
#define KEY_ENTER      5
#define KEY_ESCAPE     6
#define KEY_TAB        7
#define KEY_BACKSPACE  8
#define KEY_DELETE     9
#define KEY_HOME       10
#define KEY_END        11
#define KEY_INSERT     12
#define KEY_PAGE_UP    13
#define KEY_PAGE_DOWN  14

#define KEY_CHAR_BASE  256u

#define KEY_CHAR(ch) \
    ((uint16_t)(KEY_CHAR_BASE + (uint8_t)(ch)))

#define KEY_IS_CHAR(key) \
    ((uint16_t)(key) >= KEY_CHAR_BASE)

#define KEY_TO_CHAR(key) \
    ((char)((uint16_t)(key) - KEY_CHAR_BASE))

/*
 * Keyboard initialization.
 *
 * Must be called once during kernel startup before
 * keyboard_poll_key().
 */
void keyboard_init(void);

/*
 * Poll the keyboard.
 *
 * Returns:
 *
 *     KEY_NONE       No new key.
 *     KEY_*          Special key.
 *     KEY_CHAR(...)  ASCII character.
 *
 * This function is non-blocking.
 */
uint16_t keyboard_poll_key(void);

#endif // KEYBOARD_H