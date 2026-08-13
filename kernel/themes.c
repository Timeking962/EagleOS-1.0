// EagleOS 1.0 Theme System.

#include <stdint.h>
#include "../include/themes.h"

/*
 * Current EagleOS theme.
 *
 * These defaults are based on the colors currently being used
 * by the render backend:
 *
 * 0  = black
 * 1  = blue
 * 4  = red
 * 7  = light gray
 * 8  = dark gray
 * 14 = yellow
 * 15 = white
 */
static eos_theme_t current_theme;


/*
 * Restore EagleOS's default theme.
 */
void theme_reset_defaults(void) {
    current_theme.desktop_color = 0;

    current_theme.window_background = 7;

    current_theme.window_border = 15;

    current_theme.window_title_active = 1;
    current_theme.window_title_inactive = 8;

    current_theme.window_title_text = 15;

    current_theme.desktop_text = 15;
}


/*
 * Initialize the theme system.
 */
void theme_init(void) {
    theme_reset_defaults();
}


/*
 * Return a pointer to the current theme.
 */
const eos_theme_t *theme_get(void) {
    return &current_theme;
}


/*
 * Replace the current theme.
 */
void theme_set(const eos_theme_t *theme) {
    if (theme == 0) {
        return;
    }

    current_theme = *theme;
}


/*
 * Individual setters.
 */

void theme_set_desktop_color(uint8_t color) {
    current_theme.desktop_color = color;
}

void theme_set_window_background(uint8_t color) {
    current_theme.window_background = color;
}

void theme_set_window_border(uint8_t color) {
    current_theme.window_border = color;
}

void theme_set_window_title_active(uint8_t color) {
    current_theme.window_title_active = color;
}

void theme_set_window_title_inactive(uint8_t color) {
    current_theme.window_title_inactive = color;
}

void theme_set_window_title_text(uint8_t color) {
    current_theme.window_title_text = color;
}

void theme_set_desktop_text(uint8_t color) {
    current_theme.desktop_text = color;
}