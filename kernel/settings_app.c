// EagleOS 1.0 Settings Application.

#include <stdint.h>

#include "../include/render_backend.h"
#include "../include/keyboard.h"
#include "../include/exec.h"
#include "../include/settings.h"
#include "../include/themes.h"
#include "../include/desktop_interface_manager.h"


typedef enum {
    SETTINGS_SECTION_CURSOR = 0,
    SETTINGS_SECTION_TIME,
    SETTINGS_SECTION_DISPLAY,
    SETTINGS_SECTION_THEME,
    SETTINGS_SECTION_APPLY,
    SETTINGS_SECTION_RESET,
    SETTINGS_SECTION_CLOSE,
    SETTINGS_SECTION_COUNT
} settings_section_t;


/*
 * Built-in themes.
 *
 * All values are VGA Mode 13h palette indexes.
 */

static const eos_theme_t THEME_EAGLE = {
    0,
    7,
    15,
    1,
    8,
    15,
    15
};


static const eos_theme_t THEME_BLUE = {
    1,
    7,
    15,
    1,
    8,
    15,
    15
};


static const eos_theme_t THEME_GREEN = {
    2,
    7,
    15,
    2,
    8,
    15,
    15
};


static const eos_theme_t THEME_RED = {
    4,
    7,
    15,
    4,
    8,
    15,
    15
};


static const eos_theme_t THEME_DARK = {
    8,
    0,
    7,
    8,
    1,
    15,
    15
};


#define SETTINGS_THEME_COUNT 5


static const eos_theme_t *THEMES[SETTINGS_THEME_COUNT] = {
    &THEME_EAGLE,
    &THEME_BLUE,
    &THEME_GREEN,
    &THEME_RED,
    &THEME_DARK
};


static const char *THEME_NAMES[SETTINGS_THEME_COUNT] = {
    "EAGLE",
    "BLUE",
    "GREEN",
    "RED",
    "DARK"
};


static uint8_t selected_section = SETTINGS_SECTION_CURSOR;
static uint8_t editing = 0;
static uint8_t theme_index = 0;
static uint8_t prev_left_button = 0;


/*
 * Determine which built-in theme is currently active.
 */
static void detect_theme(void) {
    const eos_theme_t *theme = settings_get_theme();

    if (!theme) {
        theme_index = 0;
        return;
    }

    for (uint8_t i = 0; i < SETTINGS_THEME_COUNT; ++i) {
        const eos_theme_t *candidate = THEMES[i];

        if (theme->desktop_color == candidate->desktop_color &&
            theme->window_background == candidate->window_background &&
            theme->window_border == candidate->window_border &&
            theme->window_title_active == candidate->window_title_active &&
            theme->window_title_inactive == candidate->window_title_inactive &&
            theme->window_title_text == candidate->window_title_text &&
            theme->desktop_text == candidate->desktop_text) {

            theme_index = i;
            return;
        }
    }

    theme_index = 0;
}


/*
 * Cursor blink setting name.
 */
static const char *cursor_blink_name(void) {
    uint16_t value = settings_get_cursor_blink();

    if (value >= CURSOR_BLINK_VERY_SLOW) {
        return "VERY SLOW";
    }

    if (value >= CURSOR_BLINK_SLOW) {
        return "SLOW";
    }

    if (value >= CURSOR_BLINK_FAST) {
        return "FAST";
    }

    if (value >= CURSOR_BLINK_VERY_FAST) {
        return "VERY FAST";
    }

    return "NORMAL";
}


/*
 * Time format name.
 */
static const char *time_format_name(void) {
    if (settings_get_time_format() == TIME_FORMAT_24_HOUR) {
        return "24 HOUR";
    }

    return "12 HOUR";
}


/*
 * Resolution name.
 */
static const char *resolution_name(void) {
    if (settings_get_resolution() == EOS_RESOLUTION_640X480) {
        return "640 X 480";
    }

    return "320 X 200";
}


/*
 * Cursor blink previous value.
 */
static void cursor_blink_left(void) {
    uint16_t value = settings_get_cursor_blink();

    if (value == CURSOR_BLINK_VERY_SLOW) {
        value = CURSOR_BLINK_VERY_FAST;
    } else if (value == CURSOR_BLINK_SLOW) {
        value = CURSOR_BLINK_VERY_SLOW;
    } else if (value == CURSOR_BLINK_NORMAL) {
        value = CURSOR_BLINK_SLOW;
    } else if (value == CURSOR_BLINK_FAST) {
        value = CURSOR_BLINK_NORMAL;
    } else {
        value = CURSOR_BLINK_FAST;
    }

    settings_set_cursor_blink(value);
}


/*
 * Cursor blink next value.
 */
static void cursor_blink_right(void) {
    uint16_t value = settings_get_cursor_blink();

    if (value == CURSOR_BLINK_VERY_SLOW) {
        value = CURSOR_BLINK_SLOW;
    } else if (value == CURSOR_BLINK_SLOW) {
        value = CURSOR_BLINK_NORMAL;
    } else if (value == CURSOR_BLINK_NORMAL) {
        value = CURSOR_BLINK_FAST;
    } else if (value == CURSOR_BLINK_FAST) {
        value = CURSOR_BLINK_VERY_FAST;
    } else {
        value = CURSOR_BLINK_VERY_SLOW;
    }

    settings_set_cursor_blink(value);
}


/*
 * Toggle between 12 and 24 hour time.
 */
static void time_format_toggle(void) {
    if (settings_get_time_format() == TIME_FORMAT_12_HOUR) {
        settings_set_time_format(TIME_FORMAT_24_HOUR);
    } else {
        settings_set_time_format(TIME_FORMAT_12_HOUR);
    }
}


/*
 * Toggle between configured resolutions.
 */
static void resolution_toggle(void) {
    if (settings_get_resolution() == EOS_RESOLUTION_320X200) {
        settings_set_resolution(EOS_RESOLUTION_640X480);
    } else {
        settings_set_resolution(EOS_RESOLUTION_320X200);
    }
}


/*
 * Select previous theme.
 */
static void theme_left(void) {
    if (theme_index == 0) {
        theme_index = SETTINGS_THEME_COUNT - 1;
    } else {
        theme_index--;
    }

    settings_set_theme(THEMES[theme_index]);
}


/*
 * Select next theme.
 */
static void theme_right(void) {
    theme_index++;

    if (theme_index >= SETTINGS_THEME_COUNT) {
        theme_index = 0;
    }

    settings_set_theme(THEMES[theme_index]);
}


/*
 * Draw one Settings row.
 */
static void draw_setting_row(
    int y,
    const char *label,
    const char *value,
    uint8_t selected
) {
    if (selected) {
        graphics_rect(
            24,
            y - 1,
            262,
            10,
            1
        );

        graphics_text(
            27,
            y,
            ">",
            15
        );
    }

    graphics_text(
        38,
        y,
        label,
        15
    );

    if (value && value[0] != '\0') {
        graphics_text(
            166,
            y,
            value,
            selected ? 15 : 14
        );
    }
}


/*
 * Settings window drawing.
 */
static void settings_draw(void) {

    /*
     * IMPORTANT:
     *
     * This uses the 9-argument DIM window interface:
     *
     * x, y, w, h,
     * title,
     * menu_bar_x, menu_bar_y, menu_bar_w,
     * active
     */
    dim_draw_app_window(
        18,
        10,
        284,
        178,
        "SYSTEM SETTINGS",
        19,
        22,
        282,
        true
    );

    graphics_text(
        28,
        38,
        "SYSTEM SETTINGS",
        15
    );

    draw_setting_row(
        52,
        "CURSOR BLINK",
        cursor_blink_name(),
        selected_section == SETTINGS_SECTION_CURSOR
    );

    draw_setting_row(
        66,
        "TIME FORMAT",
        time_format_name(),
        selected_section == SETTINGS_SECTION_TIME
    );

    draw_setting_row(
        80,
        "RESOLUTION",
        resolution_name(),
        selected_section == SETTINGS_SECTION_DISPLAY
    );

    draw_setting_row(
        94,
        "THEME",
        THEME_NAMES[theme_index],
        selected_section == SETTINGS_SECTION_THEME
    );

    graphics_rect(
        24,
        108,
        262,
        1,
        8
    );

    draw_setting_row(
        120,
        "APPLY",
        "",
        selected_section == SETTINGS_SECTION_APPLY
    );

    draw_setting_row(
        134,
        "RESET DEFAULTS",
        "",
        selected_section == SETTINGS_SECTION_RESET
    );

    draw_setting_row(
        148,
        "CLOSE",
        "",
        selected_section == SETTINGS_SECTION_CLOSE
    );

    graphics_text(
        28,
        168,
        "ARROWS: SELECT",
        8
    );

    graphics_text(
        150,
        168,
        "ENTER: CHANGE",
        8
    );
}


/*
 * Settings application startup.
 */
static void settings_start(void) {
    selected_section = SETTINGS_SECTION_CURSOR;
    editing = 0;
    prev_left_button = 0;

    detect_theme();
}


/*
 * Activate current selection.
 */
static void activate_selected(void) {

    switch (selected_section) {

        case SETTINGS_SECTION_CURSOR:
            editing = 1;
            break;

        case SETTINGS_SECTION_TIME:
            time_format_toggle();
            break;

        case SETTINGS_SECTION_DISPLAY:
            resolution_toggle();
            break;

        case SETTINGS_SECTION_THEME:
            editing = 1;
            break;

        case SETTINGS_SECTION_APPLY:
            /*
             * Settings are already applied immediately.
             */
            editing = 0;
            break;

        case SETTINGS_SECTION_RESET:
            settings_reset_defaults();
            detect_theme();
            editing = 0;
            break;

        case SETTINGS_SECTION_CLOSE:
            exec_close_current();
            break;

        default:
            break;
    }
}


/*
 * Keyboard input.
 */
static void settings_key(uint16_t key) {

    /*
     * Editing cursor blink or theme.
     */
    if (editing) {

        if (key == KEY_ESCAPE) {
            editing = 0;
            return;
        }

        if (selected_section == SETTINGS_SECTION_CURSOR) {

            if (key == KEY_LEFT) {
                cursor_blink_left();
            } else if (key == KEY_RIGHT) {
                cursor_blink_right();
            } else if (key == KEY_ENTER) {
                editing = 0;
            }

            return;
        }

        if (selected_section == SETTINGS_SECTION_THEME) {

            if (key == KEY_LEFT) {
                theme_left();
            } else if (key == KEY_RIGHT) {
                theme_right();
            } else if (key == KEY_ENTER) {
                editing = 0;
            }

            return;
        }

        editing = 0;
    }


    if (key == KEY_UP) {

        if (selected_section > 0) {
            selected_section--;
        }

    } else if (key == KEY_DOWN) {

        if (selected_section + 1 < SETTINGS_SECTION_COUNT) {
            selected_section++;
        }

    } else if (key == KEY_LEFT) {

        if (selected_section == SETTINGS_SECTION_TIME) {
            time_format_toggle();
        } else if (selected_section == SETTINGS_SECTION_DISPLAY) {
            resolution_toggle();
        }

    } else if (key == KEY_RIGHT) {

        if (selected_section == SETTINGS_SECTION_TIME) {
            time_format_toggle();
        } else if (selected_section == SETTINGS_SECTION_DISPLAY) {
            resolution_toggle();
        }

    } else if (key == KEY_ENTER) {

        activate_selected();

    } else if (key == KEY_ESCAPE) {

        editing = 0;
    }
}


/*
 * Mouse input.
 */
static void settings_mouse(
    int16_t x,
    int16_t y,
    uint8_t left_button,
    uint8_t right_button
) {
    (void)right_button;

    uint8_t press_edge =
        (uint8_t)(left_button && !prev_left_button);

    if (press_edge) {

        if (x >= 24 && x <= 286) {

            /*
             * Cursor blink.
             */
            if (y >= 47 && y < 61) {

                selected_section = SETTINGS_SECTION_CURSOR;
                editing = 1;

                if (x < 150) {
                    cursor_blink_left();
                } else {
                    cursor_blink_right();
                }
            }

            /*
             * Time format.
             */
            else if (y >= 61 && y < 75) {

                selected_section = SETTINGS_SECTION_TIME;
                editing = 0;
                time_format_toggle();
            }

            /*
             * Resolution.
             */
            else if (y >= 75 && y < 89) {

                selected_section = SETTINGS_SECTION_DISPLAY;
                editing = 0;
                resolution_toggle();
            }

            /*
             * Theme.
             */
            else if (y >= 89 && y < 103) {

                selected_section = SETTINGS_SECTION_THEME;
                editing = 1;

                if (x < 150) {
                    theme_left();
                } else {
                    theme_right();
                }
            }

            /*
             * Apply.
             */
            else if (y >= 115 && y < 129) {

                selected_section = SETTINGS_SECTION_APPLY;
                editing = 0;
            }

            /*
             * Reset.
             */
            else if (y >= 129 && y < 143) {

                selected_section = SETTINGS_SECTION_RESET;
                activate_selected();
            }

            /*
             * Close.
             */
            else if (y >= 143 && y < 157) {

                selected_section = SETTINGS_SECTION_CLOSE;
                exec_close_current();
            }
        }
    }

    prev_left_button = left_button;
}


/*
 * Settings executable descriptor.
 */
static const executable_header_t SETTINGS_EXECUTABLE = {
    EEXE_MAGIC,
    EEXE_VERSION,
    "SETTINGS",
    settings_start,
    settings_draw,
    settings_key,
    settings_mouse
};


/*
 * Settings executable registry entry.
 */
const executable_header_t *settings_executable(void) {
    return &SETTINGS_EXECUTABLE;
}