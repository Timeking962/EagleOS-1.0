// EagleOS 1.0 Disk Installer Program.
#include <stdint.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"
#include "../include/keyboard.h"
#include "../include/exec.h"
#include "../include/disk.h"
#include "../include/system.h"

extern void serial_write_string(const char *s);

typedef enum install_state {
    INST_IDLE,
    INST_WORKING,
    INST_OK,
    INST_FAIL
} install_state_t;

static install_state_t state = INST_IDLE;
static uint16_t fail_sector = 0;
static uint8_t prev_left_button = 0;
static uint8_t help_menu_open = 0;
static uint8_t about_open = 0;

#define INSTALL_TOTAL_SECTORS 101u
#define INSTALL_HELP_X 220
#define INSTALL_HELP_Y 41
#define INSTALL_HELP_W 32
#define INSTALL_MENU_Y 53

static int install_to_primary_disk(void) {
    const uint8_t *boot_sector = (const uint8_t *)(uintptr_t)0x00007C00u;

    serial_write_string("[inst] start install to primary disk\n");

    if (!disk_write_sector(0u, boot_sector)) {
        fail_sector = 0;
        serial_write_string("[inst] fail writing boot sector\n");
        return 0;
    }

    for (uint32_t lba = 1; lba < INSTALL_TOTAL_SECTORS; ++lba) {
        const uint8_t *src = (const uint8_t *)(uintptr_t)(0x00010000u + (lba - 1u) * 512u);
        if (!disk_write_sector(lba, src)) {
            fail_sector = (uint16_t)lba;
            serial_write_string("[inst] fail writing payload sector\n");
            return 0;
        }
    }

    serial_write_string("[inst] install finished\n");
    return 1;
}

static void installer_start(void) {
    state = INST_IDLE;
    fail_sector = 0;
    prev_left_button = 0;
    help_menu_open = 0;
    about_open = 0;
}

static void draw_menu_bar(void) {
    dim_draw_menu_title(INSTALL_HELP_X, INSTALL_HELP_Y, 38, "HELP", help_menu_open);
}

static void draw_help_menu(void) {
    if (!help_menu_open) {
        return;
    }
    dim_draw_menu_dropdown_frame(214, INSTALL_MENU_Y, 64, 14);
    dim_draw_menu_dropdown_item(214, INSTALL_MENU_Y + 2, 64, "ABOUT", 0);
}

static void draw_about_dialog(void) {
    if (!about_open) {
        return;
    }

    dim_draw_about_dialog(58, 54, 194, "ABOUT INSTALL", "INSTALL");
}

static void installer_draw(void) {
    dim_draw_app_window(34, 26, 252, 146, "OS INSTALLER", 35, 38, 250, true);
    draw_menu_bar();

    graphics_text(44, 58, "TARGET: PRIMARY HDD", 15);
    graphics_text(44, 68, "WRITES LBA 0-100", 8);

    graphics_rect(52, 92, 86, 18, 2);
    graphics_box(52, 92, 86, 18, 15);
    graphics_text(66, 97, "INSTALL", 15);

    graphics_rect(152, 92, 86, 18, 8);
    graphics_box(152, 92, 86, 18, 15);
    graphics_text(182, 97, "BACK", 15);

    if (state == INST_IDLE) {
        graphics_text(44, 120, "ENTER OR CLICK INSTALL", 15);
    } else if (state == INST_WORKING) {
        graphics_text(44, 120, "INSTALLING...", 14);
    } else if (state == INST_OK) {
        graphics_text(44, 120, "INSTALL SUCCESS", 10);
        graphics_text(44, 130, "BOOT FROM HDD (run_hdd.bat)", 15);
    } else if (state == INST_FAIL) {
        graphics_text(44, 120, "INSTALL FAILED", 4);
        graphics_text(44, 130, "CHECK DISK / LBA ACCESS", 15);
    }

    graphics_text(44, 142, "H OR RMB: HELP", 8);
    draw_help_menu();
    draw_about_dialog();
}

static void start_install(void) {
    state = INST_WORKING;
    if (install_to_primary_disk()) {
        state = INST_OK;
    } else {
        state = INST_FAIL;
    }
}

static void installer_key(uint16_t key) {
    if (about_open) {
        if (key == KEY_ENTER || key == KEY_ESCAPE) {
            about_open = 0;
        }
        return;
    }

    if (help_menu_open) {
        if (key == KEY_ENTER) {
            about_open = 1;
            help_menu_open = 0;
        } else if (key == KEY_ESCAPE) {
            help_menu_open = 0;
        }
        return;
    }

    if (key == KEY_ESCAPE) {
        exec_launch("PROGMAN");
        return;
    }

    if (key == KEY_ENTER) {
        start_install();
        return;
    }

    if (KEY_IS_CHAR(key)) {
        char ch = KEY_TO_CHAR(key);
        if (ch == 'i' || ch == 'I') {
            start_install();
        } else if (ch == 'b' || ch == 'B') {
            exec_launch("PROGMAN");
        } else if (ch == 'h' || ch == 'H') {
            help_menu_open = 1;
        }
    }
}

static void installer_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button) {
    if (right_button) {
        help_menu_open = 1;
        prev_left_button = left_button;
        return;
    }

    {
        uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_edge) {
            if (about_open) {
                about_open = 0;
                prev_left_button = left_button;
                return;
            }

            if (help_menu_open) {
                if (x >= 214 && x <= 278 && y >= INSTALL_MENU_Y && y <= INSTALL_MENU_Y + 14) {
                    about_open = 1;
                }
                help_menu_open = 0;
                prev_left_button = left_button;
                return;
            }

            if (x >= INSTALL_HELP_X && x <= INSTALL_HELP_X + INSTALL_HELP_W && y >= INSTALL_HELP_Y && y <= INSTALL_HELP_Y + 10) {
                help_menu_open = 1;
                prev_left_button = left_button;
                return;
            }

            if (x >= 52 && x <= 138 && y >= 84 && y <= 102) {
                start_install();
            } else if (x >= 152 && x <= 238 && y >= 84 && y <= 102) {
                exec_launch("PROGMAN");
            }
        }
    }

    prev_left_button = left_button;
}

static const executable_header_t INSTALLER_EXECUTABLE = {
    EEXE_MAGIC,
    EEXE_VERSION,
    "INSTALL",
    installer_start,
    installer_draw,
    installer_key,
    installer_mouse
};

const executable_header_t *installer_executable(void) {
    return &INSTALLER_EXECUTABLE;
}
