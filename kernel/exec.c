// EagleOS 1.0 Executable Runtime Code.

#include <stdint.h>
#include <stdbool.h>

#include "../include/exec.h"
#include "../include/exec_native.h"
#include "../include/render_backend.h"
#include "../include/system.h"

#include "programs.h"

extern void serial_write_string(const char *s);


/* ------------------------------------------------------------------------- */
/* Configuration                                                             */
/* ------------------------------------------------------------------------- */

#define EXEC_BUILTIN_COUNT 7
#define EXEC_MAX_ACTIVE   16
#define EXEC_MAX_OPEN     16

#define EXEC_LOAD_BASE    0x00070000u
#define EXEC_LOAD_LIMIT   0x00088000u

#define OPEN_FLAG_OPEN      0x01u
#define OPEN_FLAG_MINIMIZED 0x02u


/* ------------------------------------------------------------------------- */
/* Built-in executable registry                                              */
/* ------------------------------------------------------------------------- */

typedef const executable_header_t *(*exec_getter_t)(void);

typedef struct builtin_program {
    uint16_t id;
    exec_getter_t getter;
} builtin_program_t;


/*
 * Built-in executable registry.
 *
 * IMPORTANT:
 *
 * Settings is exported as settings_executable().
 */
static const builtin_program_t builtins[EXEC_BUILTIN_COUNT] = {
    {1, program_manager_executable},
    {2, calculator_executable},
    {3, text_editor_executable},
    {4, file_manager_executable},
    {5, installer_executable},
    {6, sysver_executable},
    {7, settings_executable}
};


/* ------------------------------------------------------------------------- */
/* Active executable state                                                   */
/* ------------------------------------------------------------------------- */

static executable_header_t active_programs[EXEC_MAX_ACTIVE];

static char active_names[EXEC_MAX_ACTIVE][17];

static uint8_t active_is_native[EXEC_MAX_ACTIVE];

static uint16_t active_count = 0;

static int16_t current_index = -1;


/*
 * Kept for compatibility with the original runtime design.
 */
static const executable_header_t *current_program = 0;

static uint32_t exec_alloc_cursor = EXEC_LOAD_BASE;


/* ------------------------------------------------------------------------- */
/* Open-window ordering                                                      */
/* ------------------------------------------------------------------------- */

static uint16_t open_order[EXEC_MAX_OPEN];

static uint16_t open_count = 0;

static uint8_t open_flags[EXEC_MAX_ACTIVE];


/* ------------------------------------------------------------------------- */
/* Window geometry                                                            */
/* ------------------------------------------------------------------------- */

static int16_t window_x[EXEC_MAX_ACTIVE];
static int16_t window_y[EXEC_MAX_ACTIVE];
static int16_t window_w[EXEC_MAX_ACTIVE];
static int16_t window_h[EXEC_MAX_ACTIVE];


/*
 * Saved geometry for maximized windows.
 */
static int16_t restore_x[EXEC_MAX_ACTIVE];
static int16_t restore_y[EXEC_MAX_ACTIVE];
static int16_t restore_w[EXEC_MAX_ACTIVE];
static int16_t restore_h[EXEC_MAX_ACTIVE];


/* ------------------------------------------------------------------------- */
/* Window interaction state                                                  */
/* ------------------------------------------------------------------------- */

static int16_t maximized_index = -1;

static int16_t dragging_index = -1;

static int16_t resizing_index = -1;

static int16_t drawing_program_index = -1;

static uint8_t resizing_left_edge = 0;
static uint8_t resizing_right_edge = 0;
static uint8_t resizing_top_edge = 0;
static uint8_t resizing_bottom_edge = 0;

static int16_t drag_offset_x = 0;
static int16_t drag_offset_y = 0;

static int16_t resize_origin_mouse_x = 0;
static int16_t resize_origin_mouse_y = 0;

static int16_t resize_origin_x = 0;
static int16_t resize_origin_y = 0;

static int16_t resize_origin_w = 0;
static int16_t resize_origin_h = 0;

static uint8_t prev_left_button = 0;


/* ------------------------------------------------------------------------- */
/* Utility functions                                                         */
/* ------------------------------------------------------------------------- */

static uint8_t streq(
    const char *a,
    const char *b
) {
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


static void copy_name(
    char *dst,
    uint16_t dst_size,
    const char *src
) {
    uint16_t i = 0;

    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (
        i + 1 < dst_size &&
        src[i] != '\0'
    ) {
        dst[i] = src[i];
        ++i;
    }

    dst[i] = '\0';
}


static int16_t find_active_by_name(
    const char *name
) {
    uint16_t i;

    if (!name) {
        return -1;
    }

    for (i = 0; i < active_count; ++i) {
        if (streq(active_names[i], name)) {
            return (int16_t)i;
        }
    }

    return -1;
}


static int16_t find_open_by_active_index(
    int16_t active_index
) {
    uint16_t i;

    for (i = 0; i < open_count; ++i) {
        if (
            open_order[i] ==
            (uint16_t)active_index
        ) {
            return (int16_t)i;
        }
    }

    return -1;
}


/*
 * Remove an entry from open_order.
 */
static void remove_open_order(
    uint16_t position
) {
    uint16_t i;

    if (position >= open_count) {
        return;
    }

    for (
        i = position;
        i + 1 < open_count;
        ++i
    ) {
        open_order[i] = open_order[i + 1];
    }

    if (open_count > 0) {
        --open_count;
    }
}


/*
 * Put an active window at the front/top of the window ordering.
 */
static void focus_active_index(
    int16_t active_index
) {
    int16_t existing;
    uint16_t i;

    if (
        active_index < 0 ||
        active_index >= (int16_t)active_count
    ) {
        return;
    }

    existing =
        find_open_by_active_index(active_index);

    if (existing >= 0) {
        remove_open_order((uint16_t)existing);
    }

    if (open_count < EXEC_MAX_OPEN) {
        open_order[open_count++] =
            (uint16_t)active_index;
    }

    current_index = active_index;
    current_program = &active_programs[active_index];

    /*
     * A focused window is no longer minimized.
     */
    open_flags[active_index] &=
        (uint8_t)~OPEN_FLAG_MINIMIZED;
}


/*
 * Calculate a default window position.
 */
static void default_window_geometry(
    uint16_t index
) {
    int16_t offset;

    offset =
        (int16_t)((index % 5) * 12);

    window_x[index] =
        (int16_t)(18 + offset);

    window_y[index] =
        (int16_t)(10 + offset);

    window_w[index] = 284;
    window_h[index] = 178;
}


/*
 * Make sure a window remains on-screen.
 */
static void clamp_window(
    int16_t index
) {
    int16_t max_x;
    int16_t max_y;

    if (
        index < 0 ||
        index >= (int16_t)active_count
    ) {
        return;
    }

    max_x =
        (int16_t)(SCREEN_WIDTH - window_w[index]);

    max_y =
        (int16_t)(SCREEN_HEIGHT - 16 - window_h[index]);

    if (window_x[index] < 0) {
        window_x[index] = 0;
    }

    if (window_y[index] < 0) {
        window_y[index] = 0;
    }

    if (window_x[index] > max_x) {
        window_x[index] = max_x;
    }

    if (window_y[index] > max_y) {
        window_y[index] = max_y;
    }
}


/*
 * Delete an active executable and repair all indices.
 */
static void remove_active_index(
    uint16_t remove_index
) {
    uint16_t i;
    uint16_t old_count;

    if (
        remove_index >= active_count
    ) {
        return;
    }

    old_count = active_count;

    /*
     * Remove the executable from the open-order list first.
     */
    {
        int16_t open_position =
            find_open_by_active_index(
                (int16_t)remove_index
            );

        if (open_position >= 0) {
            remove_open_order(
                (uint16_t)open_position
            );
        }
    }

    /*
     * Every open-order index above the removed
     * active index must move down by one.
     */
    for (i = 0; i < open_count; ++i) {
        if (
            open_order[i] >
            remove_index
        ) {
            --open_order[i];
        }
    }

    /*
     * Shift active executable records.
     */
    for (
        i = remove_index;
        i + 1 < old_count;
        ++i
    ) {
        active_programs[i] =
            active_programs[i + 1];

        copy_name(
            active_names[i],
            sizeof(active_names[i]),
            active_names[i + 1]
        );

        active_is_native[i] =
            active_is_native[i + 1];

        open_flags[i] =
            open_flags[i + 1];

        window_x[i] =
            window_x[i + 1];

        window_y[i] =
            window_y[i + 1];

        window_w[i] =
            window_w[i + 1];

        window_h[i] =
            window_h[i + 1];

        restore_x[i] =
            restore_x[i + 1];

        restore_y[i] =
            restore_y[i + 1];

        restore_w[i] =
            restore_w[i + 1];

        restore_h[i] =
            restore_h[i + 1];
    }

    --active_count;

    /*
     * Repair special indices.
     */
    if (current_index == (int16_t)remove_index) {
        current_index = -1;
    } else if (
        current_index > (int16_t)remove_index
    ) {
        --current_index;
    }

    if (maximized_index == (int16_t)remove_index) {
        maximized_index = -1;
    } else if (
        maximized_index > (int16_t)remove_index
    ) {
        --maximized_index;
    }

    if (dragging_index == (int16_t)remove_index) {
        dragging_index = -1;
    } else if (
        dragging_index > (int16_t)remove_index
    ) {
        --dragging_index;
    }

    if (resizing_index == (int16_t)remove_index) {
        resizing_index = -1;
    } else if (
        resizing_index > (int16_t)remove_index
    ) {
        --resizing_index;
    }

    if (
        drawing_program_index ==
        (int16_t)remove_index
    ) {
        drawing_program_index = -1;
    } else if (
        drawing_program_index >
        (int16_t)remove_index
    ) {
        --drawing_program_index;
    }

    /*
     * Clear the now-unused final slot.
     */
    if (active_count < EXEC_MAX_ACTIVE) {
        active_names[active_count][0] =
            '\0';

        open_flags[active_count] = 0;
        active_is_native[active_count] = 0;
    }
}


/* ------------------------------------------------------------------------- */
/* Executable registry                                                       */
/* ------------------------------------------------------------------------- */

uint16_t exec_count(void) {
    return EXEC_BUILTIN_COUNT;
}


const executable_header_t *exec_get(
    uint16_t index
) {
    const executable_header_t *program;

    if (index >= EXEC_BUILTIN_COUNT) {
        return 0;
    }

    if (!builtins[index].getter) {
        return 0;
    }

    program =
        builtins[index].getter();

    if (!program) {
        return 0;
    }

    /*
     * Validate the executable descriptor.
     */
    if (
        program->magic != EEXE_MAGIC ||
        program->version != EEXE_VERSION
    ) {
        return 0;
    }

    return program;
}


/* ------------------------------------------------------------------------- */
/* Runtime initialization                                                    */
/* ------------------------------------------------------------------------- */

void exec_init(void) {
    uint16_t i;

    active_count = 0;
    current_index = -1;
    current_program = 0;

    open_count = 0;

    maximized_index = -1;
    dragging_index = -1;
    resizing_index = -1;
    drawing_program_index = -1;

    resizing_left_edge = 0;
    resizing_right_edge = 0;
    resizing_top_edge = 0;
    resizing_bottom_edge = 0;

    drag_offset_x = 0;
    drag_offset_y = 0;

    resize_origin_mouse_x = 0;
    resize_origin_mouse_y = 0;

    resize_origin_x = 0;
    resize_origin_y = 0;
    resize_origin_w = 0;
    resize_origin_h = 0;

    prev_left_button = 0;

    exec_alloc_cursor =
        EXEC_LOAD_BASE;

    for (
        i = 0;
        i < EXEC_MAX_ACTIVE;
        ++i
    ) {
        active_names[i][0] = '\0';
        active_is_native[i] = 0;
        open_flags[i] = 0;

        window_x[i] = 0;
        window_y[i] = 0;
        window_w[i] = 0;
        window_h[i] = 0;

        restore_x[i] = 0;
        restore_y[i] = 0;
        restore_w[i] = 0;
        restore_h[i] = 0;
    }

    for (
        i = 0;
        i < EXEC_MAX_OPEN;
        ++i
    ) {
        open_order[i] = 0;
    }

    serial_write_string(
        "[exec] initialized\n"
    );
}


/* ------------------------------------------------------------------------- */
/* Current executable                                                        */
/* ------------------------------------------------------------------------- */

const executable_header_t *exec_current(void) {
    if (
        current_index < 0 ||
        current_index >= (int16_t)active_count
    ) {
        return 0;
    }

    return &active_programs[current_index];
}


const char *exec_current_name(void) {
    const executable_header_t *program =
        exec_current();

    if (!program) {
        return 0;
    }

    return program->name;
}


/* ------------------------------------------------------------------------- */
/* Launching                                                                 */
/* ------------------------------------------------------------------------- */

int exec_launch(
    const char *name
) {
    uint16_t i;
    const executable_header_t *program;
    int16_t existing;

    if (!name || !name[0]) {
        serial_write_string(
            "[exec] launch failed: empty name\n"
        );

        return -1;
    }

    /*
     * If the executable is already open, simply
     * bring its existing window to the front.
     */
    existing =
        find_active_by_name(name);

    if (existing >= 0) {
        serial_write_string(
            "[exec] focusing existing window\n"
        );

        focus_active_index(existing);

        if (
            active_programs[existing].on_start
        ) {
            /*
             * Do NOT restart an already-running
             * executable.
             */
        }

        return existing;
    }

    /*
     * Search the built-in registry.
     */
    program = 0;

    for (
        i = 0;
        i < EXEC_BUILTIN_COUNT;
        ++i
    ) {
        const executable_header_t *candidate =
            exec_get(i);

        if (!candidate) {
            continue;
        }

        if (
            candidate->name &&
            streq(candidate->name, name)
        ) {
            program = candidate;
            break;
        }
    }

    if (!program) {
        serial_write_string(
            "[exec] launch failed: executable not found\n"
        );

        return -1;
    }

    /*
     * Check active-window capacity.
     */
    if (
        active_count >= EXEC_MAX_ACTIVE ||
        open_count >= EXEC_MAX_OPEN
    ) {
        serial_write_string(
            "[exec] launch failed: no window slots\n"
        );

        return -1;
    }

    /*
     * Copy the executable descriptor into the
     * active-program table.
     */
    {
        uint16_t index = active_count;

        active_programs[index] =
            *program;

        copy_name(
            active_names[index],
            sizeof(active_names[index]),
            program->name
        );

        active_is_native[index] = 0;

        open_flags[index] =
            OPEN_FLAG_OPEN;

        default_window_geometry(index);

        restore_x[index] =
            window_x[index];

        restore_y[index] =
            window_y[index];

        restore_w[index] =
            window_w[index];

        restore_h[index] =
            window_h[index];

        ++active_count;

        /*
         * Put the new executable at the front.
         */
        focus_active_index(
            (int16_t)index
        );

        /*
         * Start the executable.
         */
        if (active_programs[index].on_start) {
            active_programs[index].on_start();
        }

        serial_write_string(
            "[exec] launch successful\n"
        );

        return index;
    }
}


int exec_launch_index(
    uint16_t index
) {
    const executable_header_t *program;

    program = exec_get(index);

    if (!program || !program->name) {
        return -1;
    }

    return exec_launch(program->name);
}


/* ------------------------------------------------------------------------- */
/* Window closing                                                            */
/* ------------------------------------------------------------------------- */

void exec_close_current(void) {
    int16_t closing_index;
    int16_t next_index = -1;
    uint16_t i;

    if (
        current_index < 0 ||
        current_index >= (int16_t)active_count
    ) {
        /*
         * If there is no current program, try
         * to launch Program Manager.
         */
        exec_launch("PROGMAN");
        return;
    }

    closing_index = current_index;

    /*
     * Prefer the next-most-recent open window.
     */
    for (
        i = open_count;
        i > 0;
        --i
    ) {
        int16_t candidate =
            (int16_t)open_order[i - 1];

        if (
            candidate != closing_index &&
            candidate >= 0 &&
            candidate < (int16_t)active_count &&
            (open_flags[candidate] &
             OPEN_FLAG_OPEN)
        ) {
            next_index = candidate;
            break;
        }
    }

    remove_active_index(
        (uint16_t)closing_index
    );

    if (
        next_index >= 0 &&
        next_index < (int16_t)active_count
    ) {
        focus_active_index(next_index);
        return;
    }

    /*
     * If no application remains, open Program Manager.
     */
    current_index = -1;
    current_program = 0;

    exec_launch("PROGMAN");
}


/* ------------------------------------------------------------------------- */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------- */

void exec_draw_current(void) {
    uint16_t i;

    /*
     * Draw windows from oldest to newest.
     *
     * Applications are responsible for drawing their
     * own window contents/chrome.
     */
    for (i = 0; i < open_count; ++i) {
        int16_t index =
            (int16_t)open_order[i];

        bool active =
            (index == current_index);

        if (
            index < 0 ||
            index >= (int16_t)active_count
        ) {
            continue;
        }

        if (
            !(open_flags[index] &
              OPEN_FLAG_OPEN)
        ) {
            continue;
        }

        if (
            open_flags[index] &
            OPEN_FLAG_MINIMIZED
        ) {
            continue;
        }

        drawing_program_index = index;

        if (
            active_programs[index].on_draw
        ) {
            active_programs[index].on_draw();
        }

        drawing_program_index = -1;
    }
}


/* ------------------------------------------------------------------------- */
/* Keyboard                                                                   */
/* ------------------------------------------------------------------------- */

void exec_deliver_key(
    uint16_t key
) {
    int16_t index = current_index;

    if (
        index < 0 ||
        index >= (int16_t)active_count
    ) {
        return;
    }

    if (
        !(open_flags[index] &
          OPEN_FLAG_OPEN)
    ) {
        return;
    }

    if (
        open_flags[index] &
        OPEN_FLAG_MINIMIZED
    ) {
        return;
    }

    if (active_programs[index].on_key) {
        active_programs[index].on_key(key);
    }
}


/* ------------------------------------------------------------------------- */
/* Mouse                                                                     */
/* ------------------------------------------------------------------------- */

void exec_deliver_mouse(
    int16_t x,
    int16_t y,
    uint8_t left_button,
    uint8_t right_button
) {
    uint8_t press_edge =
        (uint8_t)(
            left_button &&
            !prev_left_button
        );

    uint16_t i;

    /*
     * Handle an active drag.
     */
    if (
        dragging_index >= 0 &&
        dragging_index < (int16_t)active_count
    ) {
        if (!left_button) {
            dragging_index = -1;
        } else {
            window_x[dragging_index] =
                (int16_t)(
                    x - drag_offset_x
                );

            window_y[dragging_index] =
                (int16_t)(
                    y - drag_offset_y
                );

            clamp_window(
                dragging_index
            );
        }

        prev_left_button = left_button;
        return;
    }


    /*
     * Handle an active resize.
     */
    if (
        resizing_index >= 0 &&
        resizing_index < (int16_t)active_count
    ) {
        if (!left_button) {
            resizing_index = -1;

            resizing_left_edge = 0;
            resizing_right_edge = 0;
            resizing_top_edge = 0;
            resizing_bottom_edge = 0;
        } else {
            int16_t dx =
                (int16_t)(
                    x -
                    resize_origin_mouse_x
                );

            int16_t dy =
                (int16_t)(
                    y -
                    resize_origin_mouse_y
                );

            int16_t nx =
                resize_origin_x;

            int16_t ny =
                resize_origin_y;

            int16_t nw =
                resize_origin_w;

            int16_t nh =
                resize_origin_h;

            if (resizing_left_edge) {
                nx =
                    (int16_t)(
                        resize_origin_x + dx
                    );

                nw =
                    (int16_t)(
                        resize_origin_w - dx
                    );
            }

            if (resizing_right_edge) {
                nw =
                    (int16_t)(
                        resize_origin_w + dx
                    );
            }

            if (resizing_top_edge) {
                ny =
                    (int16_t)(
                        resize_origin_y + dy
                    );

                nh =
                    (int16_t)(
                        resize_origin_h - dy
                    );
            }

            if (resizing_bottom_edge) {
                nh =
                    (int16_t)(
                        resize_origin_h + dy
                    );
            }

            if (nw < 120) {
                if (resizing_left_edge) {
                    nx =
                        (int16_t)(
                            resize_origin_x +
                            resize_origin_w -
                            120
                        );
                }

                nw = 120;
            }

            if (nh < 80) {
                if (resizing_top_edge) {
                    ny =
                        (int16_t)(
                            resize_origin_y +
                            resize_origin_h -
                            80
                        );
                }

                nh = 80;
            }

            window_x[resizing_index] = nx;
            window_y[resizing_index] = ny;
            window_w[resizing_index] = nw;
            window_h[resizing_index] = nh;

            clamp_window(
                resizing_index
            );
        }

        prev_left_button = left_button;
        return;
    }


    /*
     * Mouse press: find the topmost window underneath
     * the pointer.
     */
    if (press_edge) {
        int16_t hit = -1;

        for (
            i = open_count;
            i > 0;
            --i
        ) {
            int16_t index =
                (int16_t)open_order[i - 1];

            if (
                index < 0 ||
                index >= (int16_t)active_count
            ) {
                continue;
            }

            if (
                !(open_flags[index] &
                  OPEN_FLAG_OPEN)
            ) {
                continue;
            }

            if (
                open_flags[index] &
                OPEN_FLAG_MINIMIZED
            ) {
                continue;
            }

            if (
                x >= window_x[index] &&
                x < window_x[index] +
                    window_w[index] &&
                y >= window_y[index] &&
                y < window_y[index] +
                    window_h[index]
            ) {
                hit = index;
                break;
            }
        }

        if (hit >= 0) {
            /*
             * Bring clicked window to front.
             */
            focus_active_index(hit);

            /*
             * Window title bar.
             */
            if (
                y >= window_y[hit] &&
                y < window_y[hit] + 14
            ) {
                /*
                 * Close button.
                 */
                if (
                    x >=
                    window_x[hit] +
                    window_w[hit] -
                    13
                ) {
                    exec_close_current();

                    prev_left_button =
                        left_button;

                    return;
                }

                /*
                 * Maximize button.
                 */
                if (
                    x >=
                    window_x[hit] +
                    window_w[hit] -
                    25 &&
                    x <
                    window_x[hit] +
                    window_w[hit] -
                    13
                ) {
                    if (
                        maximized_index ==
                        hit
                    ) {
                        window_x[hit] =
                            restore_x[hit];

                        window_y[hit] =
                            restore_y[hit];

                        window_w[hit] =
                            restore_w[hit];

                        window_h[hit] =
                            restore_h[hit];

                        maximized_index = -1;
                    } else {
                        restore_x[hit] =
                            window_x[hit];

                        restore_y[hit] =
                            window_y[hit];

                        restore_w[hit] =
                            window_w[hit];

                        restore_h[hit] =
                            window_h[hit];

                        window_x[hit] = 0;
                        window_y[hit] = 0;

                        window_w[hit] =
                            SCREEN_WIDTH;

                        window_h[hit] =
                            SCREEN_HEIGHT - 16;

                        maximized_index = hit;
                    }

                    prev_left_button =
                        left_button;

                    return;
                }

                /*
                 * Minimize button.
                 */
                if (
                    x >=
                    window_x[hit] +
                    window_w[hit] -
                    37 &&
                    x <
                    window_x[hit] +
                    window_w[hit] -
                    25
                ) {
                    open_flags[hit] |=
                        OPEN_FLAG_MINIMIZED;

                    /*
                     * Find another window to focus.
                     */
                    current_index = -1;

                    for (
                        i = open_count;
                        i > 0;
                        --i
                    ) {
                        int16_t candidate =
                            (int16_t)
                            open_order[i - 1];

                        if (
                            candidate != hit &&
                            candidate >= 0 &&
                            candidate <
                            (int16_t)active_count &&
                            (open_flags[candidate] &
                             OPEN_FLAG_OPEN) &&
                            !(open_flags[candidate] &
                              OPEN_FLAG_MINIMIZED)
                        ) {
                            current_index =
                                candidate;

                            current_program =
                                &active_programs[
                                    candidate
                                ];

                            break;
                        }
                    }

                    prev_left_button =
                        left_button;

                    return;
                }

                /*
                 * Otherwise begin dragging.
                 */
                dragging_index = hit;

                drag_offset_x =
                    (int16_t)(
                        x - window_x[hit]
                    );

                drag_offset_y =
                    (int16_t)(
                        y - window_y[hit]
                    );

                prev_left_button =
                    left_button;

                return;
            }

            /*
             * Deliver the click to the application.
             */
            if (
                active_programs[hit].on_mouse
            ) {
                active_programs[hit].on_mouse(
                    x,
                    y,
                    left_button,
                    right_button
                );
            }

            prev_left_button =
                left_button;

            return;
        }
    }


    /*
     * No window was hit. Deliver mouse input to
     * the current executable if one exists.
     */
    if (
        current_index >= 0 &&
        current_index <
        (int16_t)active_count
    ) {
        if (
            active_programs[current_index].on_mouse
        ) {
            active_programs[current_index].on_mouse(
                x,
                y,
                left_button,
                right_button
            );
        }
    }

    prev_left_button =
        left_button;
}


/* ------------------------------------------------------------------------- */
/* Window information                                                        */
/* ------------------------------------------------------------------------- */

uint16_t exec_open_window_count(void) {
    return open_count;
}


int exec_open_window_info(
    uint16_t visible_index,
    char *name_out,
    uint16_t name_out_size,
    uint8_t *minimized_out,
    uint8_t *focused_out
) {
    uint16_t active_index;

    if (
        visible_index >= open_count
    ) {
        return -1;
    }

    active_index =
        open_order[visible_index];

    if (
        active_index >= active_count
    ) {
        return -1;
    }

    if (name_out && name_out_size > 0) {
        copy_name(
            name_out,
            name_out_size,
            active_names[active_index]
        );
    }

    if (minimized_out) {
        *minimized_out =
            (open_flags[active_index] &
             OPEN_FLAG_MINIMIZED) ? 1 : 0;
    }

    if (focused_out) {
        *focused_out =
            (active_index ==
             (uint16_t)current_index) ? 1 : 0;
    }

    return 0;
}


int exec_restore_window_by_visible_index(
    uint16_t visible_index
) {
    uint16_t active_index;

    if (
        visible_index >= open_count
    ) {
        return -1;
    }

    active_index =
        open_order[visible_index];

    if (
        active_index >= active_count
    ) {
        return -1;
    }

    open_flags[active_index] &=
        (uint8_t)~OPEN_FLAG_MINIMIZED;

    focus_active_index(
        (int16_t)active_index
    );

    return 0;
}


/* ------------------------------------------------------------------------- */
/* Current window geometry                                                   */
/* ------------------------------------------------------------------------- */

int exec_current_window_rect(
    int16_t *x_out,
    int16_t *y_out,
    int16_t *w_out,
    int16_t *h_out
) {
    if (
        current_index < 0 ||
        current_index >=
        (int16_t)active_count
    ) {
        return -1;
    }

    if (x_out) {
        *x_out =
            window_x[current_index];
    }

    if (y_out) {
        *y_out =
            window_y[current_index];
    }

    if (w_out) {
        *w_out =
            window_w[current_index];
    }

    if (h_out) {
        *h_out =
            window_h[current_index];
    }

    return 0;
}


int exec_current_window_metrics(
    int16_t *x_out,
    int16_t *y_out,
    int16_t *w_out,
    int16_t *h_out
) {
    return exec_current_window_rect(
        x_out,
        y_out,
        w_out,
        h_out
    );
}