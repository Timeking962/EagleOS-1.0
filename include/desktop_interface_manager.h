#ifndef DESKTOP_INTERFACE_MANAGER_H
#define DESKTOP_INTERFACE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * EagleOS 1.0 Desktop Interface Manager
 *
 * Centralized desktop/window UI implementation.
 *
 * The DIM owns:
 *   - desktop background
 *   - taskbar
 *   - window frames
 *   - title bars
 *   - window buttons
 *   - menu bars
 *   - dropdown menus
 *   - dialogs
 *   - application window shells
 *   - common status/footer elements
 */


/* ------------------------------------------------------------------------- */
/* Standard DIM geometry                                                     */
/* ------------------------------------------------------------------------- */

#define DIM_WINDOW_BORDER             2
#define DIM_TITLE_BAR_HEIGHT         14
#define DIM_MENU_BAR_HEIGHT          12

#define DIM_WINDOW_BUTTON_SIZE       10
#define DIM_WINDOW_BUTTON_GAP         2

#define DIM_STANDARD_STATUS_HEIGHT   12
#define DIM_STANDARD_FOOTER_HEIGHT   12


/* ------------------------------------------------------------------------- */
/* Desktop                                                                   */
/* ------------------------------------------------------------------------- */

void dim_draw_desktop_background(
    const char *build_tag
);

void dim_draw_desktop_taskbar_base(
    const char *brand
);

void dim_draw_desktop_taskbar_button(
    int x,
    int y,
    int w,
    const char *label,
    bool focused,
    bool minimized
);


/* ------------------------------------------------------------------------- */
/* Window geometry                                                           */
/* ------------------------------------------------------------------------- */

/*
 * Returns the positions of the three window buttons.
 *
 * Output order:
 *
 *   min_x, min_y
 *   max_x, max_y
 *   close_x, close_y
 */
void dim_get_window_button_rects(
    int x,
    int y,
    int w,
    int h,
    int *min_x,
    int *min_y,
    int *max_x,
    int *max_y,
    int *close_x,
    int *close_y
);


/*
 * Calculate the usable application content rectangle.
 *
 * menu_bar is non-zero when the application has a menu bar.
 */
void dim_get_content_rect(
    int x,
    int y,
    int w,
    int h,
    int menu_bar,
    int *content_x,
    int *content_y,
    int *content_w,
    int *content_h
);


/* ------------------------------------------------------------------------- */
/* Window chrome                                                             */
/* ------------------------------------------------------------------------- */

void dim_draw_window_frame(
    int x,
    int y,
    int w,
    int h
);


void dim_draw_title_bar(
    int x,
    int y,
    int w,
    const char *title,
    bool active
);


void dim_draw_window_buttons(
    int x,
    int y,
    int w,
    int h,
    bool active
);


void dim_draw_menu_bar(
    int x,
    int y,
    int w
);


void dim_draw_app_window(
    int x,
    int y,
    int w,
    int h,
    const char *title,
    int menu_bar_x,
    int menu_bar_y,
    int menu_bar_w,
    bool active
);


/* ------------------------------------------------------------------------- */
/* Menu system                                                               */
/* ------------------------------------------------------------------------- */

void dim_draw_menu_title(
    int x,
    int y,
    int highlight_w,
    const char *label,
    bool selected
);


void dim_draw_menu_dropdown_frame(
    int x,
    int y,
    int w,
    int h
);


void dim_draw_menu_dropdown_item(
    int x,
    int y,
    int w,
    const char *label,
    bool selected
);


/* ------------------------------------------------------------------------- */
/* Chrome compatibility helpers                                              */
/* ------------------------------------------------------------------------- */

void dim_draw_chrome_window_shell(
    int x,
    int y,
    int w,
    int h,
    const char *title,
    int menu_bar_x,
    int menu_bar_y,
    int menu_bar_w,
    bool active
);


void dim_draw_chrome_menu_title(
    int x,
    int y,
    int highlight_w,
    const char *label,
    bool selected
);


void dim_draw_chrome_menu_dropdown_frame(
    int x,
    int y,
    int w,
    int h
);


void dim_draw_chrome_menu_dropdown_item(
    int x,
    int y,
    int w,
    const char *label,
    bool selected
);


/* ------------------------------------------------------------------------- */
/* Dialogs                                                                   */
/* ------------------------------------------------------------------------- */

void dim_draw_about_dialog(
    int x,
    int y,
    int w,
    const char *title,
    const char *program_name
);


void dim_draw_confirm_dialog(
    int x,
    int y,
    int w,
    const char *title,
    const char *message,
    const char *yes_label,
    const char *no_label,
    bool yes_selected
);


void dim_draw_input_dialog(
    int x,
    int y,
    int w,
    const char *title,
    const char *prompt,
    const char *value,
    const char *ok_label,
    const char *cancel_label
);


/* ------------------------------------------------------------------------- */
/* Common application bars                                                   */
/* ------------------------------------------------------------------------- */

void dim_draw_editor_status_bar(
    int x,
    int y,
    int w,
    const char *status_text,
    const char *filename,
    bool word_wrap
);


void dim_draw_file_manager_footer(
    int x,
    int y,
    int w,
    const char *instructions,
    const char *status_text,
    const char *position_text
);


#endif /* DESKTOP_INTERFACE_MANAGER_H */