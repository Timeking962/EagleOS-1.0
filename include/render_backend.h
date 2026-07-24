//EagleOS 1.0 Render Backend Library.
#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

void graphics_init(void);
void graphics_fill(uint8_t color);
void graphics_putpixel(int x, int y, uint8_t color);
void graphics_rect(int x, int y, int w, int h, uint8_t color);
void graphics_box(int x, int y, int w, int h, uint8_t color);
void graphics_text(int x, int y, const char *text, uint8_t color);
void graphics_text_small(int x, int y, const char *text, uint8_t color);
void graphics_begin_window_transform(int offset_x, int offset_y, int clip_x, int clip_y, int window_w, int window_h, bool active);
void graphics_end_window_transform(void);
void graphics_draw_frame(int x, int y, int w, int h);
void graphics_draw_window(int x, int y, int w, int h, const char *title, bool active);
void graphics_draw_chrome_window_shell(int x, int y, int w, int h, const char *title, int menu_bar_x, int menu_bar_y, int menu_bar_w, bool active);
void graphics_draw_chrome_menu_title(int x, int y, int highlight_w, const char *label, bool selected);
void graphics_draw_chrome_menu_dropdown_frame(int x, int y, int w, int h);
void graphics_draw_chrome_menu_dropdown_item(int x, int y, int w, const char *label, bool selected);

#endif // RENDER_BACKEND_H
