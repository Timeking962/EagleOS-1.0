// EagleOS 1.0 System Settings.
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include "themes.h"

typedef enum {
    CURSOR_BLINK_VERY_SLOW = 1000,
    CURSOR_BLINK_SLOW      = 750,
    CURSOR_BLINK_NORMAL    = 500,
    CURSOR_BLINK_FAST      = 300,
    CURSOR_BLINK_VERY_FAST = 150
} cursor_blink_speed_t;

typedef enum {
    TIME_FORMAT_12_HOUR = 0,
    TIME_FORMAT_24_HOUR = 1
} time_format_t;

/*
 * Display resolutions.
 *
 * These are settings for now. Actual graphics-mode switching
 * will be implemented in the render backend.
 */
typedef enum {
    EOS_RESOLUTION_320X200 = 0,
    EOS_RESOLUTION_640X480 = 1
} eos_resolution_t;

typedef struct {
    uint16_t cursor_blink_ms;

    time_format_t time_format;

    eos_resolution_t resolution;

    eos_theme_t theme;
} eos_settings_t;

void settings_init(void);

const eos_settings_t *settings_get(void);

void settings_reset_defaults(void);

void settings_set_cursor_blink(uint16_t milliseconds);
uint16_t settings_get_cursor_blink(void);

void settings_set_time_format(time_format_t format);
time_format_t settings_get_time_format(void);

void settings_set_resolution(eos_resolution_t resolution);
eos_resolution_t settings_get_resolution(void);

void settings_set_theme(const eos_theme_t *theme);
const eos_theme_t *settings_get_theme(void);

void settings_reset_theme(void);

#endif /* SETTINGS_H */