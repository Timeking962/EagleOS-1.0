//EagleOS 1.0 User Interface Library.
#ifndef UI_H
#define UI_H

#include <stdint.h>

void ui_init(void);
void ui_draw(void);
void ui_process_key(uint16_t key);
void ui_process_mouse(int8_t dx, int8_t dy, uint8_t left_button, uint8_t right_button);

#endif // UI_H
