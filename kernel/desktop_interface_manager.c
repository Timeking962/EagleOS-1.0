// EagleOS 1.0 Desktop Interface Manager.

#include <stdbool.h>
#include <stdint.h>

#include "../include/desktop_interface_manager.h"
#include "../include/render_backend.h"
#include "../include/system.h"


/* ------------------------------------------------------------------------- */
/* Window geometry                                                           */
/* ------------------------------------------------------------------------- */

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
) {
    int by = y + 2;

    (void)h;

    if (close_x) {
        *close_x =
            x + w -
            DIM_WINDOW_BUTTON_SIZE -
            3;
    }

    if (close_y) {
        *close_y = by;
    }

    if (max_x) {
        *max_x =
            x + w -
            DIM_WINDOW_BUTTON_SIZE -
            3 -
            DIM_WINDOW_BUTTON_GAP -
            DIM_WINDOW_BUTTON_SIZE;
    }

    if (max_y) {
        *max_y = by;
    }

    if (min_x) {
        *min_x =
            x + w -
            DIM_WINDOW_BUTTON_SIZE -
            3 -
            (DIM_WINDOW_BUTTON_GAP * 2) -
            (DIM_WINDOW_BUTTON_SIZE * 2);
    }

    if (min_y) {
        *min_y = by;
    }
}


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
) {
    int cx = x + DIM_WINDOW_BORDER;
    int cy = y + DIM_TITLE_BAR_HEIGHT + DIM_WINDOW_BORDER;

    int cw =
        w -
        (DIM_WINDOW_BORDER * 2);

    int ch =
        h -
        DIM_TITLE_BAR_HEIGHT -
        (DIM_WINDOW_BORDER * 2);

    if (menu_bar) {
        cy += DIM_MENU_BAR_HEIGHT;
        ch -= DIM_MENU_BAR_HEIGHT;
    }

    if (cw < 0) {
        cw = 0;
    }

    if (ch < 0) {
        ch = 0;
    }

    if (content_x) {
        *content_x = cx;
    }

    if (content_y) {
        *content_y = cy;
    }

    if (content_w) {
        *content_w = cw;
    }

    if (content_h) {
        *content_h = ch;
    }
}


/* ------------------------------------------------------------------------- */
/* Window chrome                                                             */
/* ------------------------------------------------------------------------- */

void dim_draw_window_frame(
    int x,
    int y,
    int w,
    int h
) {
    graphics_box(
        x,
        y,
        w,
        h,
        15
    );

    if (w > 2 && h > 2) {
        graphics_box(
            x + 1,
            y + 1,
            w - 2,
            h - 2,
            7
        );
    }
}


void dim_draw_title_bar(
    int x,
    int y,
    int w,
    const char *title,
    bool active
) {
    uint8_t background =
        active ? 1 : 8;

    graphics_rect(
        x + 1,
        y + 1,
        w - 2,
        DIM_TITLE_BAR_HEIGHT - 2,
        background
    );

    if (title) {
        graphics_text(
            x + 6,
            y + 3,
            title,
            15
        );
    }
}


void dim_draw_window_buttons(
    int x,
    int y,
    int w,
    int h,
    bool active
) {
    int min_x = 0;
    int min_y = 0;

    int max_x = 0;
    int max_y = 0;

    int close_x = 0;
    int close_y = 0;

    (void)active;

    dim_get_window_button_rects(
        x,
        y,
        w,
        h,
        &min_x,
        &min_y,
        &max_x,
        &max_y,
        &close_x,
        &close_y
    );

    /*
     * Minimize button.
     */

    graphics_rect(
        min_x,
        min_y,
        DIM_WINDOW_BUTTON_SIZE,
        DIM_WINDOW_BUTTON_SIZE,
        7
    );

    graphics_box(
        min_x,
        min_y,
        DIM_WINDOW_BUTTON_SIZE,
        DIM_WINDOW_BUTTON_SIZE,
        15
    );

    graphics_text(
        min_x + 3,
        min_y + 2,
        "-",
        15
    );

    /*
     * Maximize button.
     */

    graphics_rect(
        max_x,
        max_y,
        DIM_WINDOW_BUTTON_SIZE,
        DIM_WINDOW_BUTTON_SIZE,
        7
    );

    graphics_box(
        max_x,
        max_y,
        DIM_WINDOW_BUTTON_SIZE,
        DIM_WINDOW_BUTTON_SIZE,
        15
    );

    graphics_text(
        max_x + 2,
        max_y + 1,
        "+",
        15
    );

    /*
     * Close button.
     */

    graphics_rect(
        close_x,
        close_y,
        DIM_WINDOW_BUTTON_SIZE,
        DIM_WINDOW_BUTTON_SIZE,
        4
    );

    graphics_box(
        close_x,
        close_y,
        DIM_WINDOW_BUTTON_SIZE,
        DIM_WINDOW_BUTTON_SIZE,
        15
    );

    graphics_text(
        close_x + 2,
        close_y + 1,
        "X",
        15
    );
}


void dim_draw_menu_bar(
    int x,
    int y,
    int w
) {
    graphics_rect(
        x,
        y,
        w,
        DIM_MENU_BAR_HEIGHT,
        7
    );

    graphics_box(
        x,
        y,
        w,
        DIM_MENU_BAR_HEIGHT,
        15
    );
}


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
) {
    dim_draw_window_frame(
        x,
        y,
        w,
        h
    );

    dim_draw_title_bar(
        x,
        y,
        w,
        title,
        active
    );

    dim_draw_window_buttons(
        x,
        y,
        w,
        h,
        active
    );

    if (menu_bar_w > 0) {
        dim_draw_menu_bar(
            menu_bar_x,
            menu_bar_y,
            menu_bar_w
        );
    }
}


/* ------------------------------------------------------------------------- */
/* Menu system                                                               */
/* ------------------------------------------------------------------------- */

void dim_draw_menu_title(
    int x,
    int y,
    int highlight_w,
    const char *label,
    bool selected
) {
    if (selected) {
        graphics_rect(
            x - 2,
            y - 2,
            highlight_w,
            DIM_MENU_BAR_HEIGHT - 2,
            8
        );
    }

    graphics_text(
        x,
        y,
        label,
        selected ? 14 : 15
    );
}


void dim_draw_menu_dropdown_frame(
    int x,
    int y,
    int w,
    int h
) {
    graphics_rect(
        x,
        y,
        w,
        h,
        7
    );

    graphics_box(
        x,
        y,
        w,
        h,
        15
    );
}


void dim_draw_menu_dropdown_item(
    int x,
    int y,
    int w,
    const char *label,
    bool selected
) {
    if (selected) {
        graphics_rect(
            x + 2,
            y,
            w - 4,
            DIM_MENU_BAR_HEIGHT - 2,
            8
        );
    }

    graphics_text(
        x + 6,
        y + 1,
        label,
        selected ? 14 : 15
    );
}


/* ------------------------------------------------------------------------- */
/* Chrome helper functions                                                   */
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
) {
    dim_draw_app_window(
        x,
        y,
        w,
        h,
        title,
        menu_bar_x,
        menu_bar_y,
        menu_bar_w,
        active
    );
}


void dim_draw_chrome_menu_title(
    int x,
    int y,
    int highlight_w,
    const char *label,
    bool selected
) {
    dim_draw_menu_title(
        x,
        y,
        highlight_w,
        label,
        selected
    );
}


void dim_draw_chrome_menu_dropdown_frame(
    int x,
    int y,
    int w,
    int h
) {
    dim_draw_menu_dropdown_frame(
        x,
        y,
        w,
        h
    );
}


void dim_draw_chrome_menu_dropdown_item(
    int x,
    int y,
    int w,
    const char *label,
    bool selected
) {
    dim_draw_menu_dropdown_item(
        x,
        y,
        w,
        label,
        selected
    );
}


/* ------------------------------------------------------------------------- */
/* Dialogs                                                                   */
/* ------------------------------------------------------------------------- */

void dim_draw_about_dialog(
    int x,
    int y,
    int w,
    const char *title,
    const char *program_name
) {
    graphics_rect(
        x,
        y,
        w,
        84,
        7
    );

    graphics_box(
        x,
        y,
        w,
        84,
        15
    );

    graphics_rect(
        x + 1,
        y + 1,
        w - 2,
        DIM_TITLE_BAR_HEIGHT - 2,
        1
    );

    graphics_text(
        x + 6,
        y + 3,
        title,
        15
    );

    graphics_text(
        x + 10,
        y + 22,
        "PROGRAM:",
        15
    );

    graphics_text(
        x + 82,
        y + 22,
        program_name,
        14
    );

    graphics_text(
        x + 10,
        y + 32,
        "VERSION:",
        15
    );

    graphics_text(
        x + 82,
        y + 32,
        system_get_version(),
        14
    );

    graphics_text(
        x + 10,
        y + 42,
        "OS:",
        15
    );

    graphics_text(
        x + 34,
        y + 42,
        system_get_version(),
        14
    );

    graphics_text(
        x + 10,
        y + 54,
        "BUILD TAG:",
        15
    );

    graphics_text_small(
        x + 10,
        y + 66,
        system_get_build_tag(),
        14
    );
}


void dim_draw_confirm_dialog(
    int x,
    int y,
    int w,
    const char *title,
    const char *message,
    const char *yes_label,
    const char *no_label,
    bool yes_selected
) {
    int yes_x = x + 14;
    int no_x = x + w - 64;

    graphics_rect(
        x,
        y,
        w,
        64,
        7
    );

    graphics_box(
        x,
        y,
        w,
        64,
        15
    );

    graphics_rect(
        x + 1,
        y + 1,
        w - 2,
        DIM_TITLE_BAR_HEIGHT - 2,
        1
    );

    graphics_text(
        x + 6,
        y + 3,
        title,
        15
    );

    graphics_text(
        x + 12,
        y + 20,
        message,
        15
    );

    graphics_rect(
        yes_x,
        y + 38,
        46,
        14,
        yes_selected ? 8 : 7
    );

    graphics_rect(
        no_x,
        y + 38,
        46,
        14,
        yes_selected ? 7 : 8
    );

    graphics_box(
        yes_x,
        y + 38,
        46,
        14,
        15
    );

    graphics_box(
        no_x,
        y + 38,
        46,
        14,
        15
    );

    graphics_text(
        yes_x + 12,
        y + 41,
        yes_label,
        yes_selected ? 14 : 15
    );

    graphics_text(
        no_x + 14,
        y + 41,
        no_label,
        yes_selected ? 15 : 14
    );
}


void dim_draw_input_dialog(
    int x,
    int y,
    int w,
    const char *title,
    const char *prompt,
    const char *value,
    const char *ok_label,
    const char *cancel_label
) {
    graphics_rect(
        x,
        y,
        w,
        62,
        7
    );

    graphics_box(
        x,
        y,
        w,
        62,
        15
    );

    graphics_rect(
        x + 1,
        y + 1,
        w - 2,
        DIM_TITLE_BAR_HEIGHT - 2,
        1
    );

    graphics_text(
        x + 6,
        y + 3,
        title,
        15
    );

    graphics_text(
        x + 8,
        y + 18,
        prompt,
        15
    );

    graphics_rect(
        x + 8,
        y + 28,
        w - 16,
        12,
        0
    );

    graphics_box(
        x + 8,
        y + 28,
        w - 16,
        12,
        15
    );

    graphics_text(
        x + 12,
        y + 30,
        value,
        10
    );

    graphics_rect(
        x + 16,
        y + 44,
        62,
        12,
        2
    );

    graphics_box(
        x + 16,
        y + 44,
        62,
        12,
        15
    );

    graphics_text(
        x + 26,
        y + 46,
        ok_label,
        15
    );

    graphics_rect(
        x + w - 86,
        y + 44,
        62,
        12,
        8
    );

    graphics_box(
        x + w - 86,
        y + 44,
        62,
        12,
        15
    );

    graphics_text(
        x + w - 74,
        y + 46,
        cancel_label,
        15
    );
}


/* ------------------------------------------------------------------------- */
/* Desktop                                                                   */
/* ------------------------------------------------------------------------- */

void dim_draw_desktop_background(
    const char *build_tag
) {
    for (
        int y = 0;
        y < SCREEN_HEIGHT - 16;
        ++y
    ) {
        uint8_t stripe =
            (uint8_t)((y / 8) & 1);

        uint8_t color =
            stripe ? 3 : 1;

        graphics_rect(
            0,
            y,
            SCREEN_WIDTH,
            1,
            color
        );
    }

    for (
        int x = 0;
        x < SCREEN_WIDTH;
        x += 12
    ) {
        for (
            int y = 0;
            y < SCREEN_HEIGHT - 16;
            y += 12
        ) {
            graphics_putpixel(
                x,
                y,
                11
            );
        }
    }

    if (build_tag) {
        graphics_text_small(
            4,
            4,
            build_tag,
            15
        );
    }
}


void dim_draw_desktop_taskbar_base(
    const char *brand
) {
    graphics_rect(
        0,
        SCREEN_HEIGHT - 16,
        SCREEN_WIDTH,
        16,
        8
    );

    graphics_box(
        0,
        SCREEN_HEIGHT - 16,
        SCREEN_WIDTH,
        16,
        15
    );

    if (brand) {
        graphics_text(
            4,
            SCREEN_HEIGHT - 12,
            brand,
            15
        );
    }
}


void dim_draw_desktop_taskbar_button(
    int x,
    int y,
    int w,
    const char *label,
    bool focused,
    bool minimized
) {
    graphics_rect(
        x,
        y,
        w,
        12,
        focused ? 2 : 7
    );

    graphics_box(
        x,
        y,
        w,
        12,
        15
    );

    if (label) {
        graphics_text(
            x + 2,
            y + 2,
            label,
            minimized ? 8 : 15
        );
    }
}


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
) {
    graphics_rect(
        x,
        y,
        w,
        DIM_STANDARD_STATUS_HEIGHT,
        8
    );

    graphics_box(
        x,
        y,
        w,
        DIM_STANDARD_STATUS_HEIGHT,
        15
    );

    if (status_text) {
        graphics_text(
            x + 4,
            y + 2,
            status_text,
            15
        );
    }

    if (filename) {
        graphics_text(
            x + 102,
            y + 2,
            "FILE:",
            14
        );

        graphics_text(
            x + 142,
            y + 2,
            filename,
            15
        );
    }

    graphics_text(
        x + w - 50,
        y + 2,
        word_wrap ? "W:ON" : "W:OFF",
        14
    );
}


void dim_draw_file_manager_footer(
    int x,
    int y,
    int w,
    const char *instructions,
    const char *status_text,
    const char *position_text
) {
    /*
     * Keep the footer entirely inside its allocated height.
     */

    graphics_rect(
        x,
        y,
        w,
        DIM_STANDARD_FOOTER_HEIGHT,
        8
    );

    graphics_box(
        x,
        y,
        w,
        DIM_STANDARD_FOOTER_HEIGHT,
        15
    );

    if (instructions) {
        graphics_text(
            x + 6,
            y + 2,
            instructions,
            15
        );
    }

    if (status_text) {
        graphics_text_small(
            x + 6,
            y + 7,
            status_text,
            14
        );
    }

    if (
        position_text &&
        position_text[0]
    ) {
        graphics_text(
            x + w - 28,
            y + 2,
            position_text,
            14
        );
    }
}