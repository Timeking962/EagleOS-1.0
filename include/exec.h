// EagleOS 1.0 Executable Runtime.
#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

#define EEXE_MAGIC 0x45455845u
#define EEXE_VERSION 1u

typedef struct executable_header {
    uint32_t magic;
    uint16_t version;
    const char *name;
    void (*on_start)(void);
    void (*on_draw)(void);
    void (*on_key)(uint16_t key);
    void (*on_mouse)(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button);
} executable_header_t;

void exec_init(void);
uint16_t exec_count(void);
const executable_header_t *exec_get(uint16_t index);
const executable_header_t *exec_current(void);
const char *exec_current_name(void);

int exec_launch(const char *name);
int exec_launch_index(uint16_t index);

void exec_draw_current(void);
void exec_deliver_key(uint16_t key);
void exec_deliver_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button);
int exec_current_window_rect(int16_t *x_out, int16_t *y_out, int16_t *w_out, int16_t *h_out);

uint16_t exec_open_window_count(void);
int exec_open_window_info(uint16_t visible_index, char *name_out, uint16_t name_out_size, uint8_t *minimized_out, uint8_t *focused_out);
int exec_restore_window_by_visible_index(uint16_t visible_index);

#endif // EXEC_H
