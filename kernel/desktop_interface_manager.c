// EagleOS 1.0 Desktop Interface Manager.
#include <stdbool.h>
#include "../include/desktop_interface_manager.h"
#include "../include/render_backend.h"
#include "../include/system.h"

void dim_draw_app_window(int x, int y, int w, int h, const char *title, int menu_bar_x, int menu_bar_y, int menu_bar_w, bool active) {
    graphics_draw_chrome_window_shell(x, y, w, h, title, menu_bar_x, menu_bar_y, menu_bar_w, active);
}

void dim_draw_menu_title(int x, int y, int highlight_w, const char *label, bool selected) {
    graphics_draw_chrome_menu_title(x, y, highlight_w, label, selected);
}

void dim_draw_menu_dropdown_frame(int x, int y, int w, int h) {
    graphics_draw_chrome_menu_dropdown_frame(x, y, w, h);
}

void dim_draw_menu_dropdown_item(int x, int y, int w, const char *label, bool selected) {
    graphics_draw_chrome_menu_dropdown_item(x, y, w, label, selected);
}

void dim_draw_about_dialog(int x, int y, int w, const char *title, const char *program_name) {
    graphics_rect(x, y, w, 84, 7);
    graphics_box(x, y, w, 84, 15);
    graphics_rect(x + 1, y + 1, w - 2, 12, 1);
    graphics_text(x + 6, y + 3, title, 15);

    graphics_text(x + 10, y + 22, "PROGRAM:", 15);
    graphics_text(x + 82, y + 22, program_name, 14);
    graphics_text(x + 10, y + 32, "VERSION:", 15);
    graphics_text(x + 82, y + 32, system_get_version(), 14);
    graphics_text(x + 10, y + 42, "OS:", 15);
    graphics_text(x + 34, y + 42, system_get_version(), 14);
    graphics_text(x + 10, y + 54, "BUILD TAG:", 15);
    graphics_text_small(x + 10, y + 66, system_get_build_tag(), 14);
}

void dim_draw_confirm_dialog(int x, int y, int w, const char *title, const char *message, const char *left_label, const char *right_label, bool left_selected) {
    int left_x = x + 14;
    int right_x = x + w - 64;

    graphics_rect(x, y, w, 64, 7);
    graphics_box(x, y, w, 64, 15);
    graphics_rect(x + 1, y + 1, w - 2, 12, 1);
    graphics_text(x + 6, y + 3, title, 15);
    graphics_text(x + 12, y + 20, message, 15);

    graphics_rect(left_x, y + 38, 46, 14, left_selected ? 8 : 7);
    graphics_rect(right_x, y + 38, 46, 14, left_selected ? 7 : 8);
    graphics_box(left_x, y + 38, 46, 14, 15);
    graphics_box(right_x, y + 38, 46, 14, 15);
    graphics_text(left_x + 12, y + 41, left_label, left_selected ? 14 : 15);
    graphics_text(right_x + 14, y + 41, right_label, left_selected ? 15 : 14);
}

void dim_draw_input_dialog(int x, int y, int w, const char *title, const char *prompt, const char *value, const char *action_label, const char *cancel_label) {
    graphics_rect(x, y, w, 62, 7);
    graphics_box(x, y, w, 62, 15);
    graphics_rect(x + 1, y + 1, w - 2, 12, 1);
    graphics_text(x + 6, y + 3, title, 15);

    graphics_text(x + 8, y + 18, prompt, 15);
    graphics_rect(x + 8, y + 28, w - 16, 12, 0);
    graphics_box(x + 8, y + 28, w - 16, 12, 15);
    graphics_text(x + 12, y + 30, value, 10);

    graphics_rect(x + 16, y + 44, 62, 12, 2);
    graphics_box(x + 16, y + 44, 62, 12, 15);
    graphics_text(x + 26, y + 46, action_label, 15);

    graphics_rect(x + w - 86, y + 44, 62, 12, 8);
    graphics_box(x + w - 86, y + 44, 62, 12, 15);
    graphics_text(x + w - 74, y + 46, cancel_label, 15);
}

void dim_draw_desktop_background(const char *build_tag) {
    for (int y = 0; y < SCREEN_HEIGHT - 16; ++y) {
        uint8_t stripe = (uint8_t)((y / 8) & 1);
        uint8_t color = stripe ? 3 : 1;
        graphics_rect(0, y, SCREEN_WIDTH, 1, color);
    }

    for (int x = 0; x < SCREEN_WIDTH; x += 12) {
        for (int y = 0; y < SCREEN_HEIGHT - 16; y += 12) {
            graphics_putpixel(x, y, 11);
        }
    }

    graphics_text_small(4, 4, build_tag, 15);
}

void dim_draw_desktop_taskbar_base(const char *brand) {
    graphics_rect(0, SCREEN_HEIGHT - 16, SCREEN_WIDTH, 16, 8);
    graphics_box(0, SCREEN_HEIGHT - 16, SCREEN_WIDTH, 16, 15);
    graphics_text(4, SCREEN_HEIGHT - 12, brand, 15);
}

void dim_draw_desktop_taskbar_button(int x, int y, int w, const char *label, bool focused, bool minimized) {
    graphics_rect(x, y, w, 12, focused ? 2 : 7);
    graphics_box(x, y, w, 12, 15);
    graphics_text(x + 2, y + 2, label, minimized ? 8 : 15);
}

void dim_draw_editor_status_bar(int x, int y, int w, const char *status_text, const char *filename, bool word_wrap) {
    graphics_rect(x, y, w, 12, 8);
    graphics_box(x, y, w, 12, 15);
    graphics_text(x + 4, y + 2, status_text, 15);
    graphics_text(x + 102, y + 2, "FILE:", 14);
    graphics_text(x + 142, y + 2, filename, 15);
    graphics_text(x + w - 50, y + 2, word_wrap ? "W:ON" : "W:OFF", 14);
}

void dim_draw_file_manager_footer(int x, int y, int w, const char *instructions, const char *status_text, const char *position_text) {
    graphics_rect(x, y, w, 22, 8);
    graphics_box(x, y, w, 22, 15);
    graphics_text(x + 6, y + 2, instructions, 15);
    graphics_text(x + 6, y + 12, status_text, 14);
    if (position_text && position_text[0]) {
        graphics_text(x + w - 28, y + 2, position_text, 14);
    }
}