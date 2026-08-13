// EagleOS 1.0 Theme Definitions.
#ifndef THEMES_H
#define THEMES_H

#include <stdint.h>

/*
 * EagleOS currently uses VGA Mode 13h.
 *
 * Colors are therefore VGA palette indexes from 0-255.
 */

typedef struct {
    uint8_t desktop_color;

    uint8_t window_background;

    uint8_t window_border;

    uint8_t window_title_active;
    uint8_t window_title_inactive;

    uint8_t window_title_text;

    uint8_t desktop_text;
} eos_theme_t;


/* Initialize the theme with EagleOS default colors. */
void theme_init(void);

/* Restore the default EagleOS theme. */
void theme_reset_defaults(void);

/* Get the current theme. */
const eos_theme_t *theme_get(void);

/* Set the entire theme. */
void theme_set(const eos_theme_t *theme);

/* Individual color setters. */
void theme_set_desktop_color(uint8_t color);
void theme_set_window_background(uint8_t color);
void theme_set_window_border(uint8_t color);
void theme_set_window_title_active(uint8_t color);
void theme_set_window_title_inactive(uint8_t color);
void theme_set_window_title_text(uint8_t color);
void theme_set_desktop_text(uint8_t color);

#endif /* THEME_H */