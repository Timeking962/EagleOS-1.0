// EagleOS 1.0 System Version Program.
#include <stdint.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"
#include "../include/keyboard.h"
#include "../include/exec.h"
#include "../include/system.h"

typedef enum sv_action {
    SV_ACT_BACK,
    SV_ACT_ABOUT
} sv_action_t;

typedef struct sv_menu_item {
    const char *label;
    sv_action_t action;
} sv_menu_item_t;

typedef struct sv_top_menu {
    const char *name;
    const sv_menu_item_t *items;
    uint8_t item_count;
} sv_top_menu_t;

static const sv_menu_item_t FILE_ITEMS[] = {
    {"BACK", SV_ACT_BACK}
};

static const sv_menu_item_t HELP_ITEMS[] = {
    {"ABOUT", SV_ACT_ABOUT}
};

static const sv_top_menu_t MENUS[] = {
    {"FILE", FILE_ITEMS, 1},
    {"HELP", HELP_ITEMS, 1}
};

static uint8_t menu_open = 0;
static uint8_t active_menu = 0;
static uint8_t active_item = 0;
static uint8_t about_open = 0;
static uint8_t prev_left_button = 0;

static uint8_t menu_x(uint8_t index) {
    return (index == 0) ? 30 : 78;
}

static void open_menu(uint8_t index) {
    menu_open = 1;
    active_menu = index;
    active_item = 0;
}

static void close_menu(void) {
    menu_open = 0;
}

static void do_action(sv_action_t action) {
    if (action == SV_ACT_BACK) {
        exec_launch("PROGMAN");
    } else if (action == SV_ACT_ABOUT) {
        about_open = 1;
    }
}

static void draw_menu_bar(void) {
    for (uint8_t i = 0; i < 2; ++i) {
        uint8_t selected = (uint8_t)(menu_open && i == active_menu);
        dim_draw_menu_title(menu_x(i), 29, 40, MENUS[i].name, selected);
    }
}

static void draw_dropdown(void) {
    if (!menu_open) {
        return;
    }

    const sv_top_menu_t *menu = &MENUS[active_menu];
    int x = menu_x(active_menu) - 4;
    int y = 38;
    int w = 96;
    int h = menu->item_count * 12 + 2;

    dim_draw_menu_dropdown_frame(x, y, w, h);

    for (uint8_t i = 0; i < menu->item_count; ++i) {
        int iy = y + 2 + i * 12;
        dim_draw_menu_dropdown_item(x, iy, w, menu->items[i].label, i == active_item);
    }
}

static void draw_about(void) {
    if (!about_open) {
        return;
    }

    dim_draw_about_dialog(62, 56, 192, "ABOUT SYSVER", "SYSVER");
}

static void sv_start(void) {
    menu_open = 0;
    active_menu = 0;
    active_item = 0;
    about_open = 0;
    prev_left_button = 0;
}

static void sv_draw(void) {
    dim_draw_app_window(24, 14, 272, 170, "SYSTEM VERSION", 24, 26, 248, true);
    draw_menu_bar();

    graphics_text(36, 52, "EAGLEOS VERSION:", 15);
    graphics_text(172, 52, system_get_version(), 14);

    graphics_text(36, 66, "BUILD TAG:", 15);
    graphics_text_small(116, 68, system_get_build_tag(), 14);

    graphics_text(36, 84, "THIS APP REPORTS SYSTEM VERSION", 8);
    graphics_text(36, 94, "AND BUILD TAG FROM KERNEL API.", 8);

    draw_dropdown();
    draw_about();
}

static void sv_key(uint16_t key) {
    if (about_open) {
        if (key == KEY_ENTER || key == KEY_ESCAPE) {
            about_open = 0;
        }
        return;
    }

    if (menu_open) {
        const sv_top_menu_t *menu = &MENUS[active_menu];
        if (key == KEY_ESCAPE) {
            close_menu();
        } else if (key == KEY_LEFT) {
            active_menu = (uint8_t)((active_menu + 1) % 2);
            active_item = 0;
        } else if (key == KEY_RIGHT) {
            active_menu = (uint8_t)((active_menu + 1) % 2);
            active_item = 0;
        } else if (key == KEY_UP) {
            active_item = (uint8_t)((active_item + menu->item_count - 1) % menu->item_count);
        } else if (key == KEY_DOWN) {
            active_item = (uint8_t)((active_item + 1) % menu->item_count);
        } else if (key == KEY_ENTER) {
            do_action(menu->items[active_item].action);
            if (!about_open) {
                close_menu();
            }
        }
        return;
    }

    if (key == KEY_ESCAPE) {
        exec_launch("PROGMAN");
        return;
    }

    if (KEY_IS_CHAR(key)) {
        char ch = KEY_TO_CHAR(key);
        if (ch == 'h' || ch == 'H') {
            open_menu(1);
        }
    }
}

static void sv_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button) {
    uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);

    if (about_open) {
        if (left_edge) {
            about_open = 0;
        }
        prev_left_button = left_button;
        return;
    }

    if (right_button) {
        open_menu(1);
        prev_left_button = left_button;
        return;
    }

    if (!left_edge) {
        prev_left_button = left_button;
        return;
    }

    if (y >= 26 && y <= 38 && x >= 24 && x <= 272) {
        for (uint8_t i = 0; i < 2; ++i) {
            int mx = menu_x(i);
            if (x >= mx - 4 && x <= mx + 36) {
                open_menu(i);
                prev_left_button = left_button;
                return;
            }
        }
        close_menu();
    }

    if (menu_open) {
        const sv_top_menu_t *menu = &MENUS[active_menu];
        int dx = menu_x(active_menu) - 4;
        int dy = 38;
        int dw = 96;
        int dh = menu->item_count * 12 + 2;
        if (x >= dx && x <= dx + dw && y >= dy && y <= dy + dh) {
            int rel = y - (dy + 2);
            if (rel >= 0) {
                uint8_t row = (uint8_t)(rel / 12);
                if (row < menu->item_count) {
                    active_item = row;
                    do_action(menu->items[row].action);
                    if (!about_open) {
                        close_menu();
                    }
                    prev_left_button = left_button;
                    return;
                }
            }
        }
        close_menu();
    }

    prev_left_button = left_button;
}

static const executable_header_t SYSVER_EXECUTABLE = {
    EEXE_MAGIC,
    EEXE_VERSION,
    "SYSVER",
    sv_start,
    sv_draw,
    sv_key,
    sv_mouse
};

const executable_header_t *sysver_executable(void) {
    return &SYSVER_EXECUTABLE;
}
