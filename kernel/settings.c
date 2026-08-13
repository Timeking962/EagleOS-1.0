// EagleOS 1.0 System Settings.

#include <stdint.h>
#include "../include/settings.h"

static eos_settings_t current_settings;

void settings_init(void) {
    settings_reset_defaults();
}

void settings_reset_defaults(void) {
    current_settings.cursor_blink_ms = CURSOR_BLINK_NORMAL;

    current_settings.time_format = TIME_FORMAT_12_HOUR;

    /*
     * EagleOS currently starts in VGA Mode 13h.
     */
    current_settings.resolution = EOS_RESOLUTION_320X200;

    theme_reset_defaults();

    current_settings.theme = *theme_get();
}

const eos_settings_t *settings_get(void) {
    return &current_settings;
}

void settings_set_cursor_blink(uint16_t milliseconds) {
    if (milliseconds == 0) {
        milliseconds = CURSOR_BLINK_NORMAL;
    }

    current_settings.cursor_blink_ms = milliseconds;
}

uint16_t settings_get_cursor_blink(void) {
    return current_settings.cursor_blink_ms;
}

void settings_set_time_format(time_format_t format) {
    if (format != TIME_FORMAT_12_HOUR &&
        format != TIME_FORMAT_24_HOUR) {
        return;
    }

    current_settings.time_format = format;
}

time_format_t settings_get_time_format(void) {
    return current_settings.time_format;
}

void settings_set_resolution(eos_resolution_t resolution) {
    if (resolution != EOS_RESOLUTION_320X200 &&
        resolution != EOS_RESOLUTION_640X480) {
        return;
    }

    current_settings.resolution = resolution;
}

eos_resolution_t settings_get_resolution(void) {
    return current_settings.resolution;
}

void settings_set_theme(const eos_theme_t *theme) {
    if (theme == 0) {
        return;
    }

    current_settings.theme = *theme;

    theme_set(theme);
}

const eos_theme_t *settings_get_theme(void) {
    return &current_settings.theme;
}

void settings_reset_theme(void) {
    theme_reset_defaults();

    current_settings.theme = *theme_get();
}