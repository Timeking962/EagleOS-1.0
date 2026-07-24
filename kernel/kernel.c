//EagleOS 1.0 Kernel Version 0.2
#include <stdint.h>
#include "../include/render_backend.h"
#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/ui.h"
#include "../include/exec.h"
#include "../include/fs.h"

extern void serial_write_string(const char *s);

/*
 * Kernel serial debug toggles:
 * - KERNEL_DEBUG_INPUT: input event names.
 * - KERNEL_DEBUG_LOOP_HEARTBEAT: periodic main-loop liveness markers.
 * - KERNEL_DEBUG_REDRAW: redraw begin/done markers around ui_draw calls.
 * - KERNEL_DEBUG_FS_SELFTEST: startup FS persistence probe/seed test.
 */
#define KERNEL_DEBUG_INPUT 1
#define KERNEL_DEBUG_LOOP_HEARTBEAT 0
#define KERNEL_DEBUG_REDRAW 0
#define KERNEL_DEBUG_FS_SELFTEST 0

static void kernel_panic(const char *msg) {
    serial_write_string("[kernel] PANIC: ");
    serial_write_string(msg);
    serial_write_string("\n");
    for (;;) {
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}

static const char *key_name(uint16_t key) {
    switch (key) {
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_ENTER: return "ENTER";
        case KEY_ESCAPE: return "ESC";
        case KEY_TAB: return "TAB";
        case KEY_BACKSPACE: return "BACKSPACE";
        default: return "UNKNOWN";
    }
}

#if KERNEL_DEBUG_FS_SELFTEST
static uint8_t bytes_equal(const uint8_t *a, const uint8_t *b, uint16_t n) {
    if (!a || !b) {
        return 0;
    }
    for (uint16_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static void kernel_fs_persistence_self_test(void) {
    static const char test_name[] = "PERSIST.TST";
    static const uint8_t test_payload[] = "EAGLE_PERSIST_V1";
    uint8_t read_buf[32];
    uint16_t read_size = 0;
    int read_ok;

    serial_write_string("[kernel] fs_selftest begin\n");

    read_ok = fs_read_file(test_name, read_buf, (uint16_t)sizeof(read_buf), &read_size);
    if (read_ok
        && read_size == (uint16_t)(sizeof(test_payload) - 1)
        && bytes_equal(read_buf, test_payload, (uint16_t)(sizeof(test_payload) - 1))) {
        serial_write_string("[kernel] fs_selftest persisted file present\n");
        return;
    }

    if (!fs_write_file(test_name, test_payload, (uint16_t)(sizeof(test_payload) - 1))) {
        serial_write_string("[kernel] fs_selftest write failed\n");
        return;
    }

    if (fs_persistence_active()) {
        serial_write_string("[kernel] fs_selftest wrote seed (disk)\n");
    } else {
        serial_write_string("[kernel] fs_selftest wrote seed (ram-only)\n");
    }
}
#endif

void kernel_main(void) {
    serial_write_string("[kernel] kernel_main reached\n");

    serial_write_string("[kernel] graphics_init\n");
    graphics_init();

    serial_write_string("[kernel] ui_init\n");
    ui_init();

    serial_write_string("[kernel] mouse_init\n");
    mouse_init();

    serial_write_string("[kernel] fs_init\n");
    fs_init();
#if KERNEL_DEBUG_FS_SELFTEST
    kernel_fs_persistence_self_test();
#endif

    serial_write_string("[kernel] exec_init\n");
    exec_init();

    serial_write_string("[kernel] launch PROGMAN\n");
    if (!exec_launch("PROGMAN")) {
        kernel_panic("exec_launch(PROGMAN) failed");
    }

    serial_write_string("[kernel] ui_draw\n");
    ui_draw();
#if KERNEL_DEBUG_REDRAW
    serial_write_string("[kernel] ui_draw done\n");
#endif

    serial_write_string("[kernel] entering input loop\n");

    uint32_t loop_ticks = 0;

    for (;;) {
        loop_ticks++;
#if KERNEL_DEBUG_LOOP_HEARTBEAT
        if ((loop_ticks & 0x1FFFF) == 0) {
            serial_write_string("[kernel] loop heartbeat\n");
        }
#endif

        uint16_t key = keyboard_poll_key();
        if (key != KEY_NONE) {
#if KERNEL_DEBUG_INPUT
            serial_write_string("[kernel] key ");
            if (KEY_IS_CHAR(key)) {
                serial_write_string("CHAR");
            } else {
                serial_write_string(key_name(key));
            }
            serial_write_string("\n");
#endif
            ui_process_key(key);
#if KERNEL_DEBUG_REDRAW
            serial_write_string("[kernel] ui_draw key begin\n");
#endif
            ui_draw();
#if KERNEL_DEBUG_REDRAW
            serial_write_string("[kernel] ui_draw key done\n");
#endif
        }

        int8_t dx = 0;
        int8_t dy = 0;
        uint8_t left_button = 0;
        uint8_t right_button = 0;
        if (mouse_poll(&dx, &dy, &left_button, &right_button)) {
            ui_process_mouse(dx, dy, left_button, right_button);
#if KERNEL_DEBUG_REDRAW
            serial_write_string("[kernel] ui_draw mouse begin\n");
#endif
            ui_draw();
#if KERNEL_DEBUG_REDRAW
            serial_write_string("[kernel] ui_draw mouse done\n");
#endif
        }

        __asm__ volatile ("nop");
    }
}
