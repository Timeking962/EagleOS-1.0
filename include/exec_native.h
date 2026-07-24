// EagleOS 1.0 Native Executable ABI.
#ifndef EXEC_NATIVE_H
#define EXEC_NATIVE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct exec_host_api {
    void (*graphics_draw_window)(int x, int y, int w, int h, const char *title, bool active);
    void (*graphics_text)(int x, int y, const char *text, uint8_t color);
    void (*graphics_rect)(int x, int y, int w, int h, uint8_t color);
    void (*graphics_box)(int x, int y, int w, int h, uint8_t color);
    int (*exec_launch)(const char *name);
} exec_host_api_t;

typedef struct exec_native_callbacks {
    void (*on_start)(void);
    void (*on_draw)(void);
    void (*on_key)(uint16_t key);
    void (*on_mouse)(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button);
} exec_native_callbacks_t;

typedef int (*exec_native_entry_t)(const exec_host_api_t *host_api, exec_native_callbacks_t *out_callbacks);

#endif // EXEC_NATIVE_H
