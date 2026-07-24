//EagleOS 1.0 User Interface Code File.
#include <stdint.h>
#include <stdbool.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"
#include "../include/ui.h"
#include "../include/exec.h"
#include "../include/system.h"

static int cursor_x = SCREEN_WIDTH / 2;
static int cursor_y = SCREEN_HEIGHT / 2;
static uint8_t prev_left_button = 0;

#define TASKBAR_X 64
#define TASKBAR_ITEM_W 62
#define TASKBAR_ITEM_GAP 4

static void copy_taskbar_label(char *dst, const char *src, uint8_t max_len) {
    uint8_t i = 0;
    if (!dst || max_len == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && i + 1 < max_len) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void draw_wallpaper(void) {
    dim_draw_desktop_background(system_get_build_tag());
}

static void draw_status_bar(void) {
    const char *program_name = exec_current_name();
    uint16_t open_count = exec_open_window_count();
    (void)program_name;

    dim_draw_desktop_taskbar_base("EagleOS");

    for (uint16_t i = 0; i < open_count; ++i) {
        int x = TASKBAR_X + (int)i * (TASKBAR_ITEM_W + TASKBAR_ITEM_GAP);
        uint8_t minimized = 0;
        uint8_t focused = 0;
        char name[10];
        char label[10];

        if (x + TASKBAR_ITEM_W > SCREEN_WIDTH - 2) {
            break;
        }

        if (!exec_open_window_info(i, name, (uint16_t)sizeof(name), &minimized, &focused)) {
            continue;
        }

        copy_taskbar_label(label, name, (uint8_t)sizeof(label));
        dim_draw_desktop_taskbar_button(x, SCREEN_HEIGHT - 14, TASKBAR_ITEM_W, label, focused != 0, minimized != 0);
    }
}

static void draw_cursor(void) {
    int x = cursor_x;
    int y = cursor_y;

    static const uint8_t cursor_mask[13][8] = {
        {1,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0},
        {1,2,2,1,0,0,0,0},
        {1,2,2,2,1,0,0,0},
        {1,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,1,0},
        {1,2,2,2,2,2,2,1},
        {1,2,2,1,1,2,2,0},
        {1,2,1,0,1,2,0,0},
        {1,1,0,0,1,0,0,0},
        {1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0}
    };

    for (int row = 0; row < 13; ++row) {
        for (int col = 0; col < 8; ++col) {
            uint8_t pixel = cursor_mask[row][col];
            if (pixel == 1) {
                graphics_putpixel(x + col, y + row, 15);
            } else if (pixel == 2) {
                graphics_putpixel(x + col, y + row, 0);
            }
        }
    }
}

static void clamp_cursor(void) {
    if (cursor_x < 0) {
        cursor_x = 0;
    }
    if (cursor_y < 0) {
        cursor_y = 0;
    }
    if (cursor_x > SCREEN_WIDTH - 1) {
        cursor_x = SCREEN_WIDTH - 1;
    }
    if (cursor_y > SCREEN_HEIGHT - 8) {
        cursor_y = SCREEN_HEIGHT - 8;
    }
}

void ui_init(void) {
    cursor_x = SCREEN_WIDTH / 2;
    cursor_y = SCREEN_HEIGHT / 2;
    prev_left_button = 0;
}

void ui_draw(void) {
    draw_wallpaper();
    exec_draw_current();
    draw_status_bar();
    draw_cursor();
}

void ui_process_key(uint16_t key) {
    exec_deliver_key(key);
}

void ui_process_mouse(int8_t dx, int8_t dy, uint8_t left_button, uint8_t right_button) {
    uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
    uint8_t handled = 0;

    cursor_x += dx;
    cursor_y -= dy;
    clamp_cursor();

    if (left_edge && cursor_y >= SCREEN_HEIGHT - 16) {
        uint16_t open_count = exec_open_window_count();
        for (uint16_t i = 0; i < open_count; ++i) {
            int x = TASKBAR_X + (int)i * (TASKBAR_ITEM_W + TASKBAR_ITEM_GAP);
            if (x + TASKBAR_ITEM_W > SCREEN_WIDTH - 2) {
                break;
            }
            if (cursor_x >= x && cursor_x <= x + TASKBAR_ITEM_W && cursor_y >= SCREEN_HEIGHT - 14 && cursor_y <= SCREEN_HEIGHT - 2) {
                (void)exec_restore_window_by_visible_index(i);
                handled = 1;
                break;
            }
        }
    }

    if (!handled) {
        exec_deliver_mouse((int16_t)cursor_x, (int16_t)cursor_y, left_button, right_button);
    }

    prev_left_button = left_button;
}
