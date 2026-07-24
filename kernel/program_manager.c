// EagleOS 1.0 Program Manager Program.
#include <stdint.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"
#include "../include/keyboard.h"
#include "../include/exec.h"
#include "../include/system.h"

typedef enum pm_action {
    PM_ACT_RUN,
    PM_ACT_RESTART,
    PM_ACT_EXIT_MENU,
    PM_ACT_ABOUT
} pm_action_t;

typedef struct pm_menu_item {
    const char *label;
    pm_action_t action;
} pm_menu_item_t;

typedef struct pm_top_menu {
    const char *name;
    const pm_menu_item_t *items;
    uint8_t item_count;
} pm_top_menu_t;

static const pm_menu_item_t FILE_ITEMS[] = {
    {"RUN", PM_ACT_RUN},
    {"RESTART OS", PM_ACT_RESTART},
    {"CLOSE", PM_ACT_EXIT_MENU}
};

static const pm_menu_item_t HELP_ITEMS[] = {
    {"ABOUT", PM_ACT_ABOUT}
};

static const pm_top_menu_t MENUS[] = {
    {"FILE", FILE_ITEMS, 3},
    {"HELP", HELP_ITEMS, 1}
};

static uint16_t launchable[8];
static uint16_t launchable_count = 0;
static uint16_t selected = 0;
static uint8_t prev_left_button = 0;
static int16_t last_click_row = -1;
static uint8_t double_click_window = 0;
static uint8_t menu_open = 0;
static uint8_t active_menu = 0;
static uint8_t active_item = 0;
static uint8_t restart_confirm_open = 0;
static uint8_t restart_confirm_yes = 1;
static uint8_t about_open = 0;

static uint8_t streq(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static void refresh_program_list(void) {
    launchable_count = 0;
    uint16_t total = exec_count();
    for (uint16_t i = 0; i < total && launchable_count < 8; ++i) {
        const executable_header_t *header = exec_get(i);
        if (!header) {
            continue;
        }
        if (streq(header->name, "PROGMAN")) {
            continue;
        }
        launchable[launchable_count++] = i;
    }
    if (selected >= launchable_count) {
        selected = 0;
    }
}

static void launch_selected(void) {
    if (launchable_count == 0) {
        return;
    }
    exec_launch_index(launchable[selected]);
}

static uint8_t menu_x(uint8_t index) {
    if (index == 0) {
        return 28;
    }
    return 72;
}

static void open_menu(void) {
    menu_open = 1;
    active_item = 0;
}

static void close_menu(void) {
    menu_open = 0;
}

static void perform_menu_action(pm_action_t action) {
    if (action == PM_ACT_RUN) {
        launch_selected();
    } else if (action == PM_ACT_RESTART) {
        restart_confirm_open = 1;
        restart_confirm_yes = 1;
    } else if (action == PM_ACT_EXIT_MENU) {
        close_menu();
    } else if (action == PM_ACT_ABOUT) {
        about_open = 1;
    }
}

static void draw_about_dialog(void) {
    if (!about_open) {
        return;
    }

    dim_draw_about_dialog(56, 54, 188, "ABOUT PROGMAN", "PROGMAN");
}

static void draw_restart_confirm(void) {
    if (!restart_confirm_open) {
        return;
    }
    dim_draw_confirm_dialog(60, 64, 136, "CONFIRM RESTART", "RESTART EAGLEOS?", "YES", "NO", restart_confirm_yes != 0);
}

static void activate_current_item(void) {
    const pm_top_menu_t *menu = &MENUS[active_menu];
    if (active_item >= menu->item_count) {
        active_item = 0;
    }
    perform_menu_action(menu->items[active_item].action);
    if (menu->items[active_item].action != PM_ACT_RESTART) {
        close_menu();
    }
}

static void draw_menu_bar(void) {
    for (uint8_t i = 0; i < 2; ++i) {
        uint8_t selected = (uint8_t)(menu_open && i == active_menu);
        dim_draw_menu_title(menu_x(i), 29, 42, MENUS[i].name, selected);
    }
}

static void draw_dropdown(void) {
    if (!menu_open) {
        return;
    }

    {
        const pm_top_menu_t *menu = &MENUS[active_menu];
        int x = menu_x(active_menu) - 4;
        int y = 38;
        int w = 120;
        int h = menu->item_count * 12 + 2;

        dim_draw_menu_dropdown_frame(x, y, w, h);

        for (uint8_t i = 0; i < menu->item_count; ++i) {
            int iy = y + 2 + i * 12;
            dim_draw_menu_dropdown_item(x, iy, w, menu->items[i].label, i == active_item);
        }
    }
}

static void pm_start(void) {
    selected = 0;
    refresh_program_list();
    prev_left_button = 0;
    last_click_row = -1;
    double_click_window = 0;
    menu_open = 0;
    active_menu = 0;
    active_item = 0;
    restart_confirm_open = 0;
    restart_confirm_yes = 1;
}

static void pm_draw(void) {
    refresh_program_list();

    dim_draw_app_window(18, 14, 220, 156, "PROGRAM MANAGER", 19, 26, 218, true);
    draw_menu_bar();
    graphics_text(28, 42, "AVAILABLE PROGRAMS:", 15);

    int y = 56;
    for (uint16_t i = 0; i < launchable_count; ++i) {
        const executable_header_t *header = exec_get(launchable[i]);
        if (!header) {
            continue;
        }
        if (i == selected) {
            graphics_rect(26, y - 1, 170, 9, 8);
            graphics_text(28, y, ">", 14);
        }
        graphics_text(40, y, header->name, 15);
        y += 10;
    }

    draw_dropdown();
    draw_restart_confirm();
    draw_about_dialog();
}

static void pm_key(uint16_t key) {
    if (about_open) {
        if (key == KEY_ENTER || key == KEY_ESCAPE) {
            about_open = 0;
        }
        return;
    }

    if (restart_confirm_open) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            restart_confirm_yes = (uint8_t)!restart_confirm_yes;
        } else if (key == KEY_ENTER) {
            if (restart_confirm_yes) {
                system_reboot();
            }
            restart_confirm_open = 0;
        } else if (key == KEY_ESCAPE) {
            restart_confirm_open = 0;
        }
        return;
    }

    if (menu_open) {
        const pm_top_menu_t *menu = &MENUS[active_menu];
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
            activate_current_item();
        }
        return;
    }

    if (key == KEY_UP) {
        if (selected > 0) {
            selected--;
        }
    } else if (key == KEY_DOWN) {
        if (selected + 1 < launchable_count) {
            selected++;
        }
    } else if (key == KEY_ENTER) {
        launch_selected();
    } else if (key == KEY_ESCAPE) {
        open_menu();
    }
}

static void pm_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button) {
    if (about_open) {
        uint8_t left_press_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_press_edge) {
            about_open = 0;
        }
        prev_left_button = left_button;
        return;
    }

    if (restart_confirm_open) {
        uint8_t left_press_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_press_edge) {
            if (x >= 74 && x <= 120 && y >= 102 && y <= 116) {
                system_reboot();
            } else if (x >= 132 && x <= 178 && y >= 102 && y <= 116) {
                restart_confirm_open = 0;
            } else if (!(x >= 60 && x <= 196 && y >= 64 && y <= 128)) {
                restart_confirm_open = 0;
            }
        }
        prev_left_button = left_button;
        return;
    }

    if (right_button) {
        open_menu();
        prev_left_button = left_button;
        return;
    }

    if (double_click_window > 0) {
        double_click_window--;
    }

    uint8_t left_press_edge = (uint8_t)(left_button && !prev_left_button);

    if (left_press_edge && y >= 26 && y <= 38 && x >= 19 && x <= 237) {
        if (x >= (int16_t)(menu_x(0) - 4) && x <= (int16_t)(menu_x(0) + 38)) {
            active_menu = 0;
            open_menu();
        } else if (x >= (int16_t)(menu_x(1) - 4) && x <= (int16_t)(menu_x(1) + 38)) {
            active_menu = 1;
            open_menu();
        } else {
            close_menu();
        }
        prev_left_button = left_button;
        return;
    }

    if (menu_open) {
        int dx = menu_x(active_menu) - 4;
        int dy = 38;
        int dw = 120;
        int dh = MENUS[active_menu].item_count * 12 + 2;
        if (left_press_edge && x >= dx && x <= dx + dw && y >= dy && y <= dy + dh) {
            int rel = y - (dy + 2);
            if (rel >= 0) {
                uint8_t row = (uint8_t)(rel / 12);
                if (row < MENUS[active_menu].item_count) {
                    active_item = row;
                    activate_current_item();
                    prev_left_button = left_button;
                    return;
                }
            }
        }
        if (left_press_edge) {
            close_menu();
        }
    }

    if (left_press_edge && x >= 26 && x <= 196 && y >= 55 && y < (int16_t)(55 + launchable_count * 10)) {
        uint16_t row = (uint16_t)((y - 55) / 10);
        if (row < launchable_count) {
            selected = row;
            if (last_click_row == (int16_t)row && double_click_window > 0) {
                launch_selected();
                double_click_window = 0;
                last_click_row = -1;
            } else {
                last_click_row = (int16_t)row;
                double_click_window = 22;
            }
        }
    }

    prev_left_button = left_button;
}

static const executable_header_t PM_EXECUTABLE = {
    EEXE_MAGIC,
    EEXE_VERSION,
    "PROGMAN",
    pm_start,
    pm_draw,
    pm_key,
    pm_mouse
};

const executable_header_t *program_manager_executable(void) {
    return &PM_EXECUTABLE;
}
