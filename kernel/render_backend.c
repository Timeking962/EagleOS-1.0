// EagleOS 1.0 Graphics Code File.
#include <stdint.h>
#include <stdbool.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"

static volatile uint8_t *const VGA_MEMORY = (uint8_t *)0xA0000;

static int window_offset_x = 0;
static int window_offset_y = 0;
static int window_clip_x = 0;
static int window_clip_y = 0;
static int window_override_w = 0;
static int window_override_h = 0;
static uint8_t window_transform_active = 0;
static uint8_t window_active_state = 1;

static const uint8_t SMALL_FONT[128][5] = {
    [' '] = {0, 0, 0, 0, 0},
    ['.'] = {0, 0, 0, 0, 2},
    ['-'] = {0, 0, 7, 0, 0},
    [':'] = {0, 2, 0, 2, 0},
    ['0'] = {7, 5, 5, 5, 7},
    ['1'] = {2, 6, 2, 2, 7},
    ['2'] = {7, 1, 7, 4, 7},
    ['3'] = {7, 1, 7, 1, 7},
    ['4'] = {5, 5, 7, 1, 1},
    ['5'] = {7, 4, 7, 1, 7},
    ['6'] = {7, 4, 7, 5, 7},
    ['7'] = {7, 1, 1, 1, 1},
    ['8'] = {7, 5, 7, 5, 7},
    ['9'] = {7, 5, 7, 1, 7},
    ['A'] = {7, 5, 7, 5, 5},
    ['B'] = {6, 5, 6, 5, 6},
    ['C'] = {7, 4, 4, 4, 7},
    ['D'] = {6, 5, 5, 5, 6},
    ['E'] = {7, 4, 6, 4, 7},
    ['F'] = {7, 4, 6, 4, 4},
    ['G'] = {7, 4, 5, 5, 7},
    ['H'] = {5, 5, 7, 5, 5},
    ['I'] = {7, 2, 2, 2, 7},
    ['J'] = {1, 1, 1, 5, 7},
    ['K'] = {5, 5, 6, 5, 5},
    ['L'] = {4, 4, 4, 4, 7},
    ['M'] = {5, 7, 7, 5, 5},
    ['N'] = {5, 7, 7, 7, 5},
    ['O'] = {7, 5, 5, 5, 7},
    ['P'] = {7, 5, 7, 4, 4},
    ['Q'] = {7, 5, 5, 7, 1},
    ['R'] = {6, 5, 6, 5, 5},
    ['S'] = {7, 4, 7, 1, 7},
    ['T'] = {7, 2, 2, 2, 2},
    ['U'] = {5, 5, 5, 5, 7},
    ['V'] = {5, 5, 5, 5, 2},
    ['W'] = {5, 5, 7, 7, 5},
    ['X'] = {5, 5, 2, 5, 5},
    ['Y'] = {5, 5, 2, 2, 2},
    ['Z'] = {7, 1, 2, 4, 7}
};

void graphics_present(void) {
    /*
     * EagleOS currently renders directly into VGA mode 0x13
     * framebuffer memory at 0xA0000.
     *
     * There is no separate back buffer to copy here, so
     * presentation is currently a no-op.
     */
}

void graphics_init(void) {
    /* VGA mode 0x13 already set by bootloader. */
}

void graphics_fill(uint8_t color) {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
        VGA_MEMORY[i] = color;
    }
}

void graphics_putpixel(int x, int y, uint8_t color) {
    if (window_transform_active) {
        x += window_offset_x;
        y += window_offset_y;

        if (x < window_clip_x ||
            x >= window_clip_x + window_override_w ||
            y < window_clip_y ||
            y >= window_clip_y + window_override_h) {
            return;
        }
    }

    if (x < 0 ||
        x >= SCREEN_WIDTH ||
        y < 0 ||
        y >= SCREEN_HEIGHT) {
        return;
    }

    VGA_MEMORY[y * SCREEN_WIDTH + x] = color;
}

void graphics_rect(
    int x,
    int y,
    int w,
    int h,
    uint8_t color
) {
    for (int row = y; row < y + h; ++row) {
        for (int col = x; col < x + w; ++col) {
            graphics_putpixel(col, row, color);
        }
    }
}

void graphics_box(
    int x,
    int y,
    int w,
    int h,
    uint8_t color
) {
    for (int col = x; col < x + w; ++col) {
        graphics_putpixel(col, y, color);
        graphics_putpixel(col, y + h - 1, color);
    }

    for (int row = y; row < y + h; ++row) {
        graphics_putpixel(x, row, color);
        graphics_putpixel(x + w - 1, row, color);
    }
}

void graphics_text(
    int x,
    int y,
    const char *text,
    uint8_t color
) {
    static const uint8_t font[256][8] = {
        ['A'] = {0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00},
        ['B'] = {0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00},
        ['C'] = {0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00},
        ['D'] = {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00},
        ['E'] = {0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00},
        ['F'] = {0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00},
        ['G'] = {0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00},
        ['H'] = {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00},
        ['I'] = {0x3C,0x10,0x10,0x10,0x10,0x10,0x3C,0x00},
        ['J'] = {0x1E,0x04,0x04,0x04,0x44,0x44,0x38,0x00},
        ['K'] = {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00},
        ['L'] = {0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00},
        ['M'] = {0x42,0x66,0x5A,0x5A,0x42,0x42,0x42,0x00},
        ['N'] = {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00},
        ['O'] = {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
        ['P'] = {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00},
        ['Q'] = {0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00},
        ['R'] = {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00},
        ['S'] = {0x3C,0x42,0x20,0x18,0x04,0x42,0x3C,0x00},
        ['T'] = {0x7E,0x10,0x10,0x10,0x10,0x10,0x10,0x00},
        ['U'] = {0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
        ['V'] = {0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00},
        ['W'] = {0x42,0x42,0x42,0x5A,0x5A,0x66,0x42,0x00},
        ['X'] = {0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00},
        ['Y'] = {0x42,0x42,0x24,0x18,0x10,0x10,0x10,0x00},
        ['Z'] = {0x7E,0x02,0x04,0x18,0x20,0x40,0x7E,0x00},

        ['0'] = {0x3C,0x46,0x4A,0x52,0x62,0x46,0x3C,0x00},
        ['1'] = {0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00},
        ['2'] = {0x3C,0x42,0x02,0x0C,0x30,0x40,0x7E,0x00},
        ['3'] = {0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00},
        ['4'] = {0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x00},
        ['5'] = {0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00},
        ['6'] = {0x3C,0x40,0x7C,0x42,0x42,0x42,0x3C,0x00},
        ['7'] = {0x7E,0x42,0x04,0x08,0x10,0x10,0x10,0x00},
        ['8'] = {0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00},
        ['9'] = {0x3C,0x42,0x42,0x3E,0x02,0x42,0x3C,0x00},

        [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        ['.'] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
        [':'] = {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
        ['!'] = {0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00},
        ['+'] = {0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00},
        ['*'] = {0x00,0x44,0x28,0x10,0x28,0x44,0x00,0x00},
        ['='] = {0x00,0x00,0x7C,0x00,0x7C,0x00,0x00,0x00},
        ['%'] = {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00},
        ['/'] = {0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00},
        ['-'] = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}
    };

    while (*text) {
        char ch = *text;

        if (ch >= 'a' && ch <= 'z') {
            ch -= 'a' - 'A';
        }

        const uint8_t *glyph = font[(uint8_t)ch];

        for (int row = 0; row < 8; ++row) {
            uint8_t mask = glyph[row];

            for (int col = 0; col < 8; ++col) {
                if (mask & (0x80 >> col)) {
                    graphics_putpixel(
                        x + col,
                        y + row,
                        color
                    );
                }
            }
        }

        x += 8;
        text++;
    }
}

void graphics_text_small(
    int x,
    int y,
    const char *text,
    uint8_t color
) {
    while (text && *text) {
        uint8_t ch = (uint8_t)*text;

        if (ch >= 'a' && ch <= 'z') {
            ch = (uint8_t)(ch - ('a' - 'A'));
        }

        for (int row = 0; row < 5; ++row) {
            uint8_t mask = SMALL_FONT[ch][row];

            for (int col = 0; col < 3; ++col) {
                if (mask & (1u << (2 - col))) {
                    graphics_putpixel(
                        x + col,
                        y + row,
                        color
                    );
                }
            }
        }

        x += 4;
        text++;
    }
}

void graphics_begin_window_transform(
    int offset_x,
    int offset_y,
    int clip_x,
    int clip_y,
    int window_w,
    int window_h,
    bool active
) {
    window_offset_x = offset_x;
    window_offset_y = offset_y;
    window_clip_x = clip_x;
    window_clip_y = clip_y;
    window_override_w = window_w;
    window_override_h = window_h;
    window_transform_active = 1;
    window_active_state = active ? 1u : 0u;
}

void graphics_end_window_transform(void) {
    window_offset_x = 0;
    window_offset_y = 0;
    window_clip_x = 0;
    window_clip_y = 0;
    window_override_w = 0;
    window_override_h = 0;
    window_transform_active = 0;
    window_active_state = 1;
}

void graphics_draw_frame(
    int x,
    int y,
    int w,
    int h
) {
    graphics_box(x, y, w, h, 15);
    graphics_box(x + 1, y + 1, w - 2, h - 2, 8);
}

void graphics_draw_window(
    int x,
    int y,
    int w,
    int h,
    const char *title,
    bool active
) {
    int draw_w = w;
    int draw_h = h;
    uint8_t draw_active = active ? 1u : 0u;

    if (window_transform_active) {
        if (window_override_w > 0) {
            draw_w = window_override_w;
        }

        if (window_override_h > 0) {
            draw_h = window_override_h;
        }

        draw_active = window_active_state;
    }

    graphics_rect(x, y, draw_w, draw_h, 7);
    graphics_box(x, y, draw_w, draw_h, 15);

    if (draw_h >= 16) {
        graphics_rect(
            x + 1,
            y + 1,
            draw_w - 2,
            12,
            draw_active ? 1 : 8
        );

        graphics_text(
            x + 4,
            y + 3,
            title,
            15
        );

        if (draw_w >= 42) {
            int min_x = x + draw_w - 37;
            int max_x = x + draw_w - 25;
            int close_x = x + draw_w - 13;
            int by = y + 2;

            graphics_rect(
                min_x,
                by,
                10,
                10,
                7
            );

            graphics_box(
                min_x,
                by,
                10,
                10,
                15
            );

            graphics_text(
                min_x + 3,
                by + 2,
                "_",
                15
            );

            graphics_rect(
                max_x,
                by,
                10,
                10,
                7
            );

            graphics_box(
                max_x,
                by,
                10,
                10,
                15
            );

            graphics_text(
                max_x + 2,
                by + 2,
                "O",
                15
            );

            graphics_rect(
                close_x,
                by,
                10,
                10,
                4
            );

            graphics_box(
                close_x,
                by,
                10,
                10,
                15
            );

            graphics_text(
                close_x + 2,
                by + 2,
                "X",
                15
            );
        }

        if (draw_w >= 20 &&
            draw_h >= 24) {

            int grip_x = x + draw_w - 8;
            int grip_y = y + draw_h - 8;

            graphics_putpixel(
                grip_x + 4,
                grip_y + 6,
                15
            );

            graphics_putpixel(
                grip_x + 5,
                grip_y + 5,
                15
            );

            graphics_putpixel(
                grip_x + 6,
                grip_y + 4,
                15
            );

            graphics_putpixel(
                grip_x + 2,
                grip_y + 6,
                8
            );

            graphics_putpixel(
                grip_x + 3,
                grip_y + 5,
                8
            );

            graphics_putpixel(
                grip_x + 4,
                grip_y + 4,
                8
            );

            graphics_putpixel(
                grip_x + 5,
                grip_y + 3,
                8
            );

            graphics_putpixel(
                grip_x + 6,
                grip_y + 2,
                8
            );
        }
    }
}

void graphics_draw_chrome_window_shell(
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
    graphics_draw_window(
        x,
        y,
        w,
        h,
        title,
        active
    );

    graphics_rect(
        menu_bar_x,
        menu_bar_y,
        menu_bar_w,
        12,
        7
    );

    graphics_box(
        menu_bar_x,
        menu_bar_y,
        menu_bar_w,
        12,
        15
    );

    graphics_rect(
        menu_bar_x + 1,
        menu_bar_y + 12,
        menu_bar_w - 2,
        1,
        8
    );
}

void graphics_draw_chrome_menu_title(
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
            10,
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

void graphics_draw_chrome_menu_dropdown_frame(
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

void graphics_draw_chrome_menu_dropdown_item(
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
            10,
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