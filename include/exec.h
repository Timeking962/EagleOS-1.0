// EagleOS 1.0 Executable Runtime Interface.
#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

#define EEXE_MAGIC   0x45584545u
#define EEXE_VERSION 1u

typedef void (*executable_start_fn)(void);
typedef void (*executable_draw_fn)(void);
typedef void (*executable_key_fn)(uint16_t key);
typedef void (*executable_mouse_fn)(
    int16_t x,
    int16_t y,
    uint8_t left_button,
    uint8_t right_button
);

typedef struct executable_header {
    uint32_t magic;
    uint16_t version;

    const char *name;

    executable_start_fn on_start;
    executable_draw_fn on_draw;
    executable_key_fn on_key;
    executable_mouse_fn on_mouse;
} executable_header_t;


/*
 * Initialize the executable system.
 */
void exec_init(void);


/*
 * Executable registry.
 */
uint16_t exec_count(void);

const executable_header_t *exec_get(uint16_t index);


/*
 * Current executable.
 */
const executable_header_t *exec_current(void);

const char *exec_current_name(void);


/*
 * Launch an executable by name.
 */
int exec_launch(const char *name);


/*
 * Launch an executable by registry index.
 */
int exec_launch_index(uint16_t index);


/*
 * Close the currently focused executable/window.
 *
 * If the currently focused application is closed,
 * the next available open application is selected.
 *
 * If no application remains open, PROGMAN is launched.
 */
void exec_close_current(void);


/*
 * Draw the current executable/window set.
 */
void exec_draw_current(void);


/*
 * Deliver keyboard input.
 */
void exec_deliver_key(uint16_t key);


/*
 * Deliver mouse input.
 */
void exec_deliver_mouse(
    int16_t x,
    int16_t y,
    uint8_t left_button,
    uint8_t right_button
);


/*
 * Window information.
 */
uint16_t exec_open_window_count(void);

int exec_open_window_info(
    uint16_t visible_index,
    char *name_out,
    uint16_t name_out_size,
    uint8_t *minimized_out,
    uint8_t *focused_out
);

int exec_restore_window_by_visible_index(uint16_t visible_index);


/*
 * Current window dimensions.
 */
int exec_current_window_rect(
    int16_t *x_out,
    int16_t *y_out,
    int16_t *w_out,
    int16_t *h_out
);

int exec_current_window_metrics(
    int16_t *x_out,
    int16_t *y_out,
    int16_t *w_out,
    int16_t *h_out
);

#endif /* EXEC_H */