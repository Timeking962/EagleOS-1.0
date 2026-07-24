#ifndef DESKTOP_INTERFACE_MANAGER_H
#define DESKTOP_INTERFACE_MANAGER_H

#include <stdbool.h>

void dim_draw_app_window(int x, int y, int w, int h, const char *title, int menu_bar_x, int menu_bar_y, int menu_bar_w, bool active);
void dim_draw_menu_title(int x, int y, int highlight_w, const char *label, bool selected);
void dim_draw_menu_dropdown_frame(int x, int y, int w, int h);
void dim_draw_menu_dropdown_item(int x, int y, int w, const char *label, bool selected);
void dim_draw_about_dialog(int x, int y, int w, const char *title, const char *program_name);
void dim_draw_confirm_dialog(int x, int y, int w, const char *title, const char *message, const char *left_label, const char *right_label, bool left_selected);
void dim_draw_input_dialog(int x, int y, int w, const char *title, const char *prompt, const char *value, const char *action_label, const char *cancel_label);
void dim_draw_desktop_background(const char *build_tag);
void dim_draw_desktop_taskbar_base(const char *brand);
void dim_draw_desktop_taskbar_button(int x, int y, int w, const char *label, bool focused, bool minimized);
void dim_draw_editor_status_bar(int x, int y, int w, const char *status_text, const char *filename, bool word_wrap);
void dim_draw_file_manager_footer(int x, int y, int w, const char *instructions, const char *status_text, const char *position_text);

#endif // DESKTOP_INTERFACE_MANAGER_H