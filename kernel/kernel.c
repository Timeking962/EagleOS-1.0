// EagleOS 1.0 Kernel Version 0.2

#include <stdint.h>

#include "../include/render_backend.h"
#include "../include/keyboard.h"
#include "../include/mouse.h"
#include "../include/ui.h"
#include "../include/exec.h"
#include "../include/fs.h"
#include "../include/datetime.h"
#include "../include/timer.h"
#include "../include/settings.h"

extern void serial_write_string(const char *s);

/*
 * Kernel serial debug toggles:
 *
 * KERNEL_DEBUG_INPUT:
 *     Log keyboard input events.
 *
 * KERNEL_DEBUG_LOOP_HEARTBEAT:
 *     Periodic main-loop liveness markers.
 *
 * KERNEL_DEBUG_REDRAW:
 *     Redraw begin/done markers around ui_draw().
 *
 * KERNEL_DEBUG_FS_SELFTEST:
 *     Startup filesystem persistence test.
 */

#define KERNEL_DEBUG_INPUT 1
#define KERNEL_DEBUG_LOOP_HEARTBEAT 0
#define KERNEL_DEBUG_REDRAW 1
#define KERNEL_DEBUG_FS_SELFTEST 0
#define KERNEL_DEBUG_MOUSE 1

/* ------------------------------------------------------------
 * KERNEL PANIC
 * ------------------------------------------------------------ */

static void kernel_panic(const char *msg)
{
    serial_write_string("[kernel] PANIC: ");
    serial_write_string(msg);
    serial_write_string("\n");

    for (;;) {
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}


/* ------------------------------------------------------------
 * KEY NAME
 * ------------------------------------------------------------ */

static const char *key_name(uint16_t key)
{
    switch (key) {
        case KEY_UP:
            return "UP";

        case KEY_DOWN:
            return "DOWN";

        case KEY_LEFT:
            return "LEFT";

        case KEY_RIGHT:
            return "RIGHT";

        case KEY_ENTER:
            return "ENTER";

        case KEY_ESCAPE:
            return "ESC";

        case KEY_TAB:
            return "TAB";

        case KEY_BACKSPACE:
            return "BACKSPACE";

        default:
            return "UNKNOWN";
    }
}


/* ------------------------------------------------------------
 * FILESYSTEM SELF TEST
 * ------------------------------------------------------------ */

#if KERNEL_DEBUG_FS_SELFTEST

static uint8_t bytes_equal(
    const uint8_t *a,
    const uint8_t *b,
    uint16_t n
)
{
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


static void kernel_fs_persistence_self_test(void)
{
    static const char test_name[] = "PERSIST.TST";
    static const uint8_t test_payload[] = "EAGLE_PERSIST_V1";

    uint8_t read_buf[32];
    uint16_t read_size = 0;
    int read_ok;

    serial_write_string("[kernel] fs_selftest begin\n");

    read_ok = fs_read_file(
        test_name,
        read_buf,
        (uint16_t)sizeof(read_buf),
        &read_size
    );

    if (
        read_ok &&
        read_size == (uint16_t)(sizeof(test_payload) - 1) &&
        bytes_equal(
            read_buf,
            test_payload,
            (uint16_t)(sizeof(test_payload) - 1)
        )
    ) {
        serial_write_string(
            "[kernel] fs_selftest persisted file present\n"
        );

        return;
    }

    if (
        !fs_write_file(
            test_name,
            test_payload,
            (uint16_t)(sizeof(test_payload) - 1)
        )
    ) {
        serial_write_string(
            "[kernel] fs_selftest write failed\n"
        );

        return;
    }

    if (fs_persistence_active()) {
        serial_write_string(
            "[kernel] fs_selftest wrote seed (disk)\n"
        );
    } else {
        serial_write_string(
            "[kernel] fs_selftest wrote seed (ram-only)\n"
        );
    }
}

#endif


/* ------------------------------------------------------------
 * KERNEL MAIN
 * ------------------------------------------------------------ */

void kernel_main(void)
{
    serial_write_string("[kernel] kernel_main reached\n");


    /* --------------------------------------------------------
     * Graphics
     * -------------------------------------------------------- */

    serial_write_string("[kernel] graphics_init\n");

    graphics_init();


    /* --------------------------------------------------------
     * UI
     * -------------------------------------------------------- */

    serial_write_string("[kernel] ui_init\n");

    ui_init();


    /* --------------------------------------------------------
     * Keyboard
     *
     * IMPORTANT:
     * keyboard_init() is implemented in keyboard.c.
     * It is NOT implemented here.
     * -------------------------------------------------------- */

    serial_write_string("[kernel] keyboard_init\n");

    keyboard_init();


    /* --------------------------------------------------------
     * Mouse
     * -------------------------------------------------------- */

    serial_write_string("[kernel] mouse_init\n");

    mouse_init();


    /* --------------------------------------------------------
     * Filesystem
     * -------------------------------------------------------- */

    serial_write_string("[kernel] fs_init\n");

    fs_init();


#if KERNEL_DEBUG_FS_SELFTEST

    kernel_fs_persistence_self_test();

#endif


    /* --------------------------------------------------------
     * Settings
     * -------------------------------------------------------- */

    serial_write_string("[kernel] settings_init\n");

    settings_init();


    /* --------------------------------------------------------
     * Date / Time
     * -------------------------------------------------------- */

    serial_write_string("[kernel] datetime_init\n");

    datetime_init();


    /* --------------------------------------------------------
     * Timer
     *
     * Temporarily disabled while debugging the input system.
     * -------------------------------------------------------- */

    /*
    serial_write_string("[kernel] timer_init\n");
    timer_init();
    */


    /* --------------------------------------------------------
     * Executable Runtime
     * -------------------------------------------------------- */

    serial_write_string("[kernel] exec_init\n");

    exec_init();


    /* --------------------------------------------------------
     * Launch Program Manager
     * -------------------------------------------------------- */

    serial_write_string("[kernel] launch PROGMAN\n");

	if (exec_launch("PROGMAN") != 0) {
		kernel_panic("exec_launch(PROGMAN) failed");
	}


    /* --------------------------------------------------------
     * Initial Draw
     * -------------------------------------------------------- */

    serial_write_string("[kernel] ui_draw\n");

#if KERNEL_DEBUG_REDRAW
    serial_write_string("[kernel] redraw begin\n");
#endif

    ui_draw();

#if KERNEL_DEBUG_REDRAW
    serial_write_string("[kernel] redraw done\n");
#endif


    /* --------------------------------------------------------
     * Main Input Loop
     * -------------------------------------------------------- */

    serial_write_string("[kernel] entering input loop\n");

    uint32_t loop_ticks = 0;

    for (;;) {

        loop_ticks++;


        /* ----------------------------------------------------
         * Keyboard
         *
         * keyboard_poll_key() is implemented in keyboard.c.
         * ---------------------------------------------------- */

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
            serial_write_string("[kernel] redraw begin\n");
#endif

            ui_draw();

#if KERNEL_DEBUG_REDRAW
            serial_write_string("[kernel] redraw done\n");
#endif
        }


        /* ----------------------------------------------------
         * Mouse
         * ---------------------------------------------------- */

        int8_t dx = 0;
        int8_t dy = 0;

        uint8_t left_button = 0;
        uint8_t right_button = 0;

#if KERNEL_DEBUG_MOUSE
serial_write_string("[kernel] mouse_poll begin\n");
#endif

if (
    mouse_poll(
        &dx,
        &dy,
        &left_button,
        &right_button
    )
) {

#if KERNEL_DEBUG_MOUSE
    serial_write_string("[kernel] mouse event\n");
#endif

    ui_process_mouse(
        dx,
        dy,
        left_button,
        right_button
    );

        if (
            mouse_poll(
                &dx,
                &dy,
                &left_button,
                &right_button
            )
        ) {

            serial_write_string("[kernel] MOUSE EVENT\n");

            ui_process_mouse(
                dx,
                dy,
                left_button,
                right_button
            );

            serial_write_string("[kernel] MOUSE UI DONE\n");

            ui_draw();

            serial_write_string("[kernel] MOUSE DRAW DONE\n");
        }


        /* ----------------------------------------------------
         * Timer
         *
         * Temporarily disabled while debugging.
         * ---------------------------------------------------- */

        /*
        timer_poll();
        */


#if KERNEL_DEBUG_LOOP_HEARTBEAT

        if ((loop_ticks & 0xFFFFF) == 0) {
            serial_write_string("[kernel] loop alive\n");
        }

#endif


        /*
         * Prevent the compiler from optimizing the loop
         * into something unexpected.
         */

        __asm__ volatile ("nop");
    }
    }
}