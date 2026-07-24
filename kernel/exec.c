// EagleOS 1.0 Executable Runtime Code.
#include <stdint.h>
#include <stdbool.h>
#include "../include/exec.h"
#include "../include/exec_native.h"
#include "../include/render_backend.h"
#include "programs.h"

extern void serial_write_string(const char *s);

#define EXEC_BUILTIN_COUNT 6
#define EXEC_MAX_ACTIVE 16
#define EXEC_MAX_OPEN 16
#define EXEC_CATALOG_MAGIC 0x54414345u
#define EXEC_CATALOG_VERSION 2u
#define EXEC_CATALOG_SCAN_BASE 0x10000u
#define EXEC_CATALOG_SCAN_SIZE (100u * 512u)
#define EXEC_APP_MAGIC 0x50504145u
#define EXEC_APP_VERSION 1u
#define EXEC_LOAD_BASE 0x00070000u
#define EXEC_LOAD_LIMIT 0x00088000u

#define CAT_FLAG_BUILTIN_BRIDGE 0x0001u
#define CAT_FLAG_NATIVE_ENTRY   0x0002u

typedef const executable_header_t *(*exec_getter_t)(void);

typedef struct builtin_program {
    uint16_t id;
    exec_getter_t getter;
} builtin_program_t;

typedef struct __attribute__((packed)) disk_catalog_header {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint32_t bundle_size;
    uint32_t blob_base_offset;
} disk_catalog_header_t;

typedef struct __attribute__((packed)) disk_catalog_entry {
    uint16_t program_id;
    char name[16];
    uint32_t blob_offset;
    uint32_t blob_size;
    uint16_t flags;
    uint16_t reserved;
} disk_catalog_entry_t;

typedef struct __attribute__((packed)) disk_app_header {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t image_size;
    uint32_t entry_offset;
    uint32_t reloc_offset;
    uint32_t reloc_count;
    uint32_t bss_size;
    char name[16];
} disk_app_header_t;

static builtin_program_t const builtins[] = {
    {1, program_manager_executable},
    {2, calculator_executable},
    {3, text_editor_executable},
    {4, file_manager_executable},
    {5, installer_executable},
    {6, sysver_executable}
};

static exec_getter_t const fallback_registry[] = {
    program_manager_executable,
    calculator_executable,
    text_editor_executable,
    file_manager_executable,
    installer_executable,
    sysver_executable
};

static executable_header_t active_programs[EXEC_MAX_ACTIVE];
static char active_names[EXEC_MAX_ACTIVE][17];
static uint8_t active_is_native[EXEC_MAX_ACTIVE];
static uint16_t active_count = 0;
static const executable_header_t *current_program = 0;
static uint32_t exec_alloc_cursor = EXEC_LOAD_BASE;
static uint16_t open_order[EXEC_MAX_OPEN];
static uint16_t open_count = 0;
static uint8_t open_flags[EXEC_MAX_ACTIVE];
static int16_t window_x[EXEC_MAX_ACTIVE];
static int16_t window_y[EXEC_MAX_ACTIVE];
static int16_t window_w[EXEC_MAX_ACTIVE];
static int16_t window_h[EXEC_MAX_ACTIVE];
static int16_t restore_x[EXEC_MAX_ACTIVE];
static int16_t restore_y[EXEC_MAX_ACTIVE];
static int16_t restore_w[EXEC_MAX_ACTIVE];
static int16_t restore_h[EXEC_MAX_ACTIVE];
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

#define OPEN_FLAG_OPEN 0x01u
#define OPEN_FLAG_MINIMIZED 0x02u

static const exec_host_api_t HOST_API = {
    graphics_draw_window,
    graphics_text,
    graphics_rect,
    graphics_box,
    exec_launch
};

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

static int16_t current_window_index_for_query(void) {
    if (drawing_program_index >= 0) {
        return drawing_program_index;
    }
    return current_program_index();
}

static uint8_t window_rect_for_program(const char *name, int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
    if (!name || !x || !y || !w || !h) {
        return 0;
    }

    if (streq(name, "PROGMAN")) {
        *x = 18; *y = 14; *w = 220; *h = 156; return 1;
    }
    if (streq(name, "EDITOR")) {
        *x = 16; *y = 10; *w = 288; *h = 176; return 1;
    }
    if (streq(name, "FILEMAN")) {
        *x = 20; *y = 14; *w = 280; *h = 172; return 1;
    }
    if (streq(name, "CALC")) {
        *x = 52; *y = 14; *w = 216; *h = 150; return 1;
    }
    if (streq(name, "INSTALL")) {
        *x = 34; *y = 26; *w = 252; *h = 146; return 1;
    }
    if (streq(name, "SYSVER")) {
        *x = 24; *y = 14; *w = 272; *h = 170; return 1;
    }

    *x = 16; *y = 10; *w = 280; *h = 170;
    return 1;
}

static void maximize_rect(int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
    if (!x || !y || !w || !h) {
        return;
    }
    *x = 2;
    *y = 10;
    *w = SCREEN_WIDTH - 4;
    *h = SCREEN_HEIGHT - 26;
}

static void set_window_defaults(uint16_t index, const char *name) {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;

    if (!window_rect_for_program(name, &x, &y, &w, &h)) {
        x = 16;
        y = 10;
        w = 280;
        h = 170;
    }

    window_x[index] = x;
    window_y[index] = y;
    window_w[index] = w;
    window_h[index] = h;
    restore_x[index] = x;
    restore_y[index] = y;
    restore_w[index] = w;
    restore_h[index] = h;
}

static void get_default_window_rect(uint16_t index, int16_t *x, int16_t *y, int16_t *w, int16_t *h) {
    if (index >= active_count) {
        return;
    }
    (void)window_rect_for_program(active_programs[index].name, x, y, w, h);
}

static void translate_mouse_to_program(uint16_t index, int16_t *x, int16_t *y) {
    int16_t base_x = 0;
    int16_t base_y = 0;
    int16_t base_w = 0;
    int16_t base_h = 0;

    if (!x || !y || index >= active_count) {
        return;
    }

    get_default_window_rect(index, &base_x, &base_y, &base_w, &base_h);
    *x = (int16_t)(*x - window_x[index] + base_x);
    *y = (int16_t)(*y - window_y[index] + base_y);
}

static void move_window(uint16_t index, int16_t x, int16_t y) {
    int16_t max_x;
    int16_t max_y;

    if (index >= active_count || maximized_index == (int16_t)index) {
        return;
    }

    max_x = (int16_t)(SCREEN_WIDTH - window_w[index]);
    max_y = (int16_t)(SCREEN_HEIGHT - 28);

    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x > max_x) {
        x = max_x;
    }
    if (y > max_y) {
        y = max_y;
    }

    window_x[index] = x;
    window_y[index] = y;
    restore_x[index] = x;
    restore_y[index] = y;
}

static void min_window_rect_for_program(uint16_t index, int16_t *w, int16_t *h) {
    int16_t base_x = 0;
    int16_t base_y = 0;
    int16_t base_w = 0;
    int16_t base_h = 0;

    if (!w || !h || index >= active_count) {
        return;
    }

    get_default_window_rect(index, &base_x, &base_y, &base_w, &base_h);
    *w = (int16_t)(base_w - 40);
    *h = (int16_t)(base_h - 30);
    if (*w < 140) {
        *w = 140;
    }
    if (*h < 96) {
        *h = 96;
    }
}

static void resize_window(uint16_t index, int16_t width, int16_t height) {
    int16_t min_w = 140;
    int16_t min_h = 96;
    int16_t max_w = (int16_t)(SCREEN_WIDTH - window_x[index]);
    int16_t max_h = (int16_t)((SCREEN_HEIGHT - 16) - window_y[index]);

    if (index >= active_count || maximized_index == (int16_t)index) {
        return;
    }

    min_window_rect_for_program(index, &min_w, &min_h);
    if (width < min_w) {
        width = min_w;
    }
    if (height < min_h) {
        height = min_h;
    }
    if (max_w < min_w) {
        max_w = min_w;
    }
    if (max_h < min_h) {
        max_h = min_h;
    }
    if (width > max_w) {
        width = max_w;
    }
    if (height > max_h) {
        height = max_h;
    }

    window_w[index] = width;
    window_h[index] = height;
    restore_w[index] = width;
    restore_h[index] = height;
}

static void resize_window_edges(uint16_t index, int16_t mouse_x, int16_t mouse_y) {
    int16_t min_w = 140;
    int16_t min_h = 96;
    int16_t new_x = resize_origin_x;
    int16_t new_y = resize_origin_y;
    int16_t new_w = resize_origin_w;
    int16_t new_h = resize_origin_h;
    int16_t screen_right = SCREEN_WIDTH;
    int16_t screen_bottom = (int16_t)(SCREEN_HEIGHT - 16);

    if (index >= active_count || maximized_index == (int16_t)index) {
        return;
    }

    min_window_rect_for_program(index, &min_w, &min_h);

    if (resizing_left_edge) {
        new_x = mouse_x - drag_offset_x;
        new_w = (int16_t)(resize_origin_w + (resize_origin_x - new_x));
        if (new_w < min_w) {
            new_w = min_w;
            new_x = (int16_t)(resize_origin_x + resize_origin_w - new_w);
        }
        if (new_x < 0) {
            new_x = 0;
            new_w = (int16_t)(resize_origin_x + resize_origin_w - new_x);
        }
    }

    if (resizing_top_edge) {
        new_y = mouse_y - drag_offset_y;
        new_h = (int16_t)(resize_origin_h + (resize_origin_y - new_y));
        if (new_h < min_h) {
            new_h = min_h;
            new_y = (int16_t)(resize_origin_y + resize_origin_h - new_h);
        }
        if (new_y < 0) {
            new_y = 0;
            new_h = (int16_t)(resize_origin_y + resize_origin_h - new_y);
        }
    }

    if (resizing_right_edge) {
        new_w = (int16_t)(resize_origin_w + (mouse_x - resize_origin_mouse_x));
    }

    if (resizing_bottom_edge) {
        new_h = (int16_t)(resize_origin_h + (mouse_y - resize_origin_mouse_y));
    }

    if (new_w < min_w) {
        new_w = min_w;
    }
    if (new_h < min_h) {
        new_h = min_h;
    }
    if (new_x + new_w > screen_right) {
        new_w = (int16_t)(screen_right - new_x);
    }
    if (new_y + new_h > screen_bottom) {
        new_h = (int16_t)(screen_bottom - new_y);
    }
    if (new_w < min_w) {
        new_w = min_w;
        if (resizing_left_edge) {
            new_x = (int16_t)(screen_right - new_w);
        }
    }
    if (new_h < min_h) {
        new_h = min_h;
        if (resizing_top_edge) {
            new_y = (int16_t)(screen_bottom - new_h);
        }
    }

    window_x[index] = new_x;
    window_y[index] = new_y;
    resize_window(index, new_w, new_h);
    restore_x[index] = new_x;
    restore_y[index] = new_y;
}

static void toggle_maximize_program(uint16_t index) {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;

    if (index >= active_count) {
        return;
    }

    if (maximized_index == (int16_t)index) {
        window_x[index] = restore_x[index];
        window_y[index] = restore_y[index];
        window_w[index] = restore_w[index];
        window_h[index] = restore_h[index];
        maximized_index = -1;
        return;
    }

    if (maximized_index >= 0 && (uint16_t)maximized_index < active_count) {
        uint16_t prev = (uint16_t)maximized_index;
        window_x[prev] = restore_x[prev];
        window_y[prev] = restore_y[prev];
        window_w[prev] = restore_w[prev];
        window_h[prev] = restore_h[prev];
    }

    restore_x[index] = window_x[index];
    restore_y[index] = window_y[index];
    restore_w[index] = window_w[index];
    restore_h[index] = window_h[index];

    maximize_rect(&x, &y, &w, &h);
    window_x[index] = x;
    window_y[index] = y;
    window_w[index] = w;
    window_h[index] = h;
    maximized_index = (int16_t)index;
}

static int16_t find_open_order_position(uint16_t program_index) {
    for (uint16_t i = 0; i < open_count; ++i) {
        if (open_order[i] == program_index) {
            return (int16_t)i;
        }
    }
    return -1;
}

static void set_current_by_index(int16_t index) {
    if (index < 0 || (uint16_t)index >= active_count) {
        current_program = 0;
        return;
    }
    current_program = &active_programs[(uint16_t)index];
}

static void focus_program(uint16_t program_index) {
    int16_t pos = find_open_order_position(program_index);
    if (pos >= 0) {
        uint16_t keep = open_order[(uint16_t)pos];
        for (uint16_t i = (uint16_t)pos; i + 1 < open_count; ++i) {
            open_order[i] = open_order[i + 1];
        }
        open_order[open_count - 1] = keep;
    } else if (open_count < EXEC_MAX_OPEN) {
        open_order[open_count++] = program_index;
    }

    open_flags[program_index] |= OPEN_FLAG_OPEN;
    open_flags[program_index] &= (uint8_t)~OPEN_FLAG_MINIMIZED;
    set_current_by_index((int16_t)program_index);
}

static void select_top_visible(void) {
    if (maximized_index >= 0) {
        if ((uint16_t)maximized_index < active_count && (open_flags[(uint16_t)maximized_index] & OPEN_FLAG_OPEN) && !(open_flags[(uint16_t)maximized_index] & OPEN_FLAG_MINIMIZED)) {
            set_current_by_index(maximized_index);
            return;
        }
        maximized_index = -1;
    }

    for (int i = (int)open_count - 1; i >= 0; --i) {
        uint16_t idx = open_order[i];
        if (!(open_flags[idx] & OPEN_FLAG_OPEN)) {
            continue;
        }
        if (open_flags[idx] & OPEN_FLAG_MINIMIZED) {
            continue;
        }
        set_current_by_index((int16_t)idx);
        return;
    }

    current_program = 0;
}

static void close_program(uint16_t program_index) {
    int16_t pos = find_open_order_position(program_index);
    if (pos >= 0) {
        for (uint16_t i = (uint16_t)pos; i + 1 < open_count; ++i) {
            open_order[i] = open_order[i + 1];
        }
        if (open_count > 0) {
            open_count--;
        }
    }

    open_flags[program_index] = 0;
    if (maximized_index == (int16_t)program_index) {
        maximized_index = -1;
    }

    select_top_visible();
    if (!current_program) {
        (void)exec_launch("PROGMAN");
    }
}

static int16_t current_program_index(void) {
    if (!current_program) {
        return -1;
    }
    for (uint16_t i = 0; i < active_count; ++i) {
        if (&active_programs[i] == current_program) {
            return (int16_t)i;
        }
    }
    return -1;
}

static uint8_t valid_header(const executable_header_t *header) {
    if (!header) {
        return 0;
    }
    if (header->magic != EEXE_MAGIC || header->version != EEXE_VERSION) {
        return 0;
    }
    return 1;
}

static uint8_t valid_native_callbacks(const exec_native_callbacks_t *cb) {
    if (!cb) {
        return 0;
    }
    if (!cb->on_draw || !cb->on_key || !cb->on_mouse) {
        return 0;
    }
    return 1;
}

static void mem_copy(uint8_t *dst, const uint8_t *src, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i) {
        dst[i] = src[i];
    }
}

static uint32_t align_up(uint32_t value, uint32_t align) {
    uint32_t mask = align - 1u;
    return (value + mask) & ~mask;
}

static uint32_t alloc_region(uint32_t size, uint32_t align) {
    uint32_t start = align_up(exec_alloc_cursor, align);
    uint32_t end = start + size;
    if (end < start || end > EXEC_LOAD_LIMIT) {
        return 0;
    }
    exec_alloc_cursor = end;
    return start;
}

static const executable_header_t *find_builtin_by_id(uint16_t id) {
    for (uint16_t i = 0; i < EXEC_BUILTIN_COUNT; ++i) {
        if (builtins[i].id != id) {
            continue;
        }
        const executable_header_t *header = builtins[i].getter();
        if (valid_header(header)) {
            return header;
        }
    }
    return 0;
}

static void apply_relocations(uint32_t load_base, uint8_t *image, uint32_t image_size, const disk_app_header_t *app) {
    if (!app || app->reloc_count == 0) {
        return;
    }

    uint32_t table_bytes = app->reloc_count * sizeof(uint32_t);
    if (app->reloc_offset > image_size || table_bytes > (image_size - app->reloc_offset)) {
        return;
    }

    uint32_t *table = (uint32_t *)(void *)(image + app->reloc_offset);
    for (uint32_t i = 0; i < app->reloc_count; ++i) {
        uint32_t off = table[i];
        if (off > image_size - sizeof(uint32_t)) {
            continue;
        }
        uint32_t *patch = (uint32_t *)(void *)(image + off);
        *patch += load_base;
    }
}

static void copy_name(char *dst, const char *src, uint16_t max_len) {
    uint16_t i = 0;
    if (!dst || max_len == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (src[i] && i + 1 < max_len) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void copy_disk_name(char *dst, const char *src, uint16_t max_len) {
    uint16_t i = 0;
    if (!dst || max_len == 0) {
        return;
    }
    while (i + 1 < max_len && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void add_active_program(const executable_header_t *base, const char *name_override) {
    if (!base || active_count >= EXEC_MAX_ACTIVE) {
        return;
    }

    active_programs[active_count] = *base;
    if (name_override && name_override[0] != '\0') {
        copy_name(active_names[active_count], name_override, 17);
    } else {
        copy_name(active_names[active_count], base->name, 17);
    }
    active_programs[active_count].name = active_names[active_count];
    active_is_native[active_count] = 0;
    set_window_defaults(active_count, base->name);
    active_count++;
}

static uint8_t active_contains_name(const char *name) {
    if (!name) {
        return 0;
    }

    for (uint16_t i = 0; i < active_count; ++i) {
        if (streq(active_programs[i].name, name)) {
            return 1;
        }
    }
    return 0;
}

static void ensure_core_programs_present(void) {
    for (uint16_t i = 0; i < EXEC_BUILTIN_COUNT; ++i) {
        const executable_header_t *base = builtins[i].getter();
        if (!valid_header(base)) {
            continue;
        }
        if (!active_contains_name(base->name)) {
            add_active_program(base, 0);
        }
    }
}

static void add_active_native(const char *name, const exec_native_callbacks_t *cb) {
    if (!name || !cb || active_count >= EXEC_MAX_ACTIVE) {
        return;
    }

    active_programs[active_count].magic = EEXE_MAGIC;
    active_programs[active_count].version = EEXE_VERSION;
    copy_name(active_names[active_count], name, 17);
    active_programs[active_count].name = active_names[active_count];
    active_programs[active_count].on_start = cb->on_start;
    active_programs[active_count].on_draw = cb->on_draw;
    active_programs[active_count].on_key = cb->on_key;
    active_programs[active_count].on_mouse = cb->on_mouse;
    active_is_native[active_count] = 1;
    set_window_defaults(active_count, name);

    serial_write_string("[exec] native registered: ");
    serial_write_string(active_programs[active_count].name);
    serial_write_string("\n");

    active_count++;
}

static void load_app_blob(const disk_catalog_entry_t *entry, const uint8_t *blob_base, uint32_t blob_region_size) {
    if (!entry || !blob_base) {
        return;
    }

    if (entry->blob_size < sizeof(disk_app_header_t) || entry->blob_offset > blob_region_size) {
        return;
    }
    if (entry->blob_size > (blob_region_size - entry->blob_offset)) {
        return;
    }

    const uint8_t *blob = blob_base + entry->blob_offset;
    const disk_app_header_t *app = (const disk_app_header_t *)(const void *)blob;
    if (app->magic != EXEC_APP_MAGIC || app->version != EXEC_APP_VERSION) {
        return;
    }

    uint32_t alloc_size = app->image_size + app->bss_size;
    if (alloc_size == 0) {
        return;
    }

    if (app->image_size > entry->blob_size - sizeof(disk_app_header_t)) {
        return;
    }

    uint32_t load_base = alloc_region(alloc_size, 16u);
    if (load_base == 0) {
        return;
    }

    uint8_t *dst = (uint8_t *)(uintptr_t)load_base;
    const uint8_t *src = blob + sizeof(disk_app_header_t);
    mem_copy(dst, src, app->image_size);
    for (uint32_t i = app->image_size; i < alloc_size; ++i) {
        dst[i] = 0;
    }

    apply_relocations(load_base, dst, app->image_size, app);
}

static uint8_t load_native_app(const disk_catalog_entry_t *entry, const uint8_t *blob_base, uint32_t blob_region_size, const char *name_override) {
    if (!entry || !blob_base || !name_override) {
        return 0;
    }

    if (entry->blob_size < sizeof(disk_app_header_t) || entry->blob_offset > blob_region_size) {
        return 0;
    }
    if (entry->blob_size > (blob_region_size - entry->blob_offset)) {
        return 0;
    }

    const uint8_t *blob = blob_base + entry->blob_offset;
    const disk_app_header_t *app = (const disk_app_header_t *)(const void *)blob;
    if (app->magic != EXEC_APP_MAGIC || app->version != EXEC_APP_VERSION) {
        return 0;
    }
    if (app->image_size == 0 || app->entry_offset >= app->image_size) {
        return 0;
    }
    if (app->image_size > entry->blob_size - sizeof(disk_app_header_t)) {
        return 0;
    }

    uint32_t alloc_size = app->image_size + app->bss_size;
    uint32_t load_base = alloc_region(alloc_size, 16u);
    if (load_base == 0) {
        return 0;
    }

    uint8_t *dst = (uint8_t *)(uintptr_t)load_base;
    const uint8_t *src = blob + sizeof(disk_app_header_t);
    mem_copy(dst, src, app->image_size);
    for (uint32_t i = app->image_size; i < alloc_size; ++i) {
        dst[i] = 0;
    }

    apply_relocations(load_base, dst, app->image_size, app);

    exec_native_entry_t entry_fn = (exec_native_entry_t)(uintptr_t)(load_base + app->entry_offset);
    exec_native_callbacks_t callbacks;
    callbacks.on_start = 0;
    callbacks.on_draw = 0;
    callbacks.on_key = 0;
    callbacks.on_mouse = 0;

    if (entry_fn(&HOST_API, &callbacks) != 0) {
        return 0;
    }
    if (!valid_native_callbacks(&callbacks)) {
        return 0;
    }

    add_active_native(name_override, &callbacks);
    return 1;
}

static uint8_t load_catalog_from_disk(void) {
    uint8_t *scan = (uint8_t *)(uintptr_t)EXEC_CATALOG_SCAN_BASE;
    uint32_t limit = EXEC_CATALOG_SCAN_SIZE;

    if (limit < sizeof(disk_catalog_header_t)) {
        return 0;
    }

    for (uint32_t off = 0; off + sizeof(disk_catalog_header_t) <= limit; ++off) {
        disk_catalog_header_t *header = (disk_catalog_header_t *)(void *)(scan + off);
        if (header->magic != EXEC_CATALOG_MAGIC || header->version != EXEC_CATALOG_VERSION) {
            continue;
        }

        if (header->count == 0) {
            return 0;
        }

        if (header->bundle_size < sizeof(disk_catalog_header_t)) {
            return 0;
        }
        if (header->bundle_size > (limit - off)) {
            return 0;
        }

        uint32_t table_size = (uint32_t)header->count * (uint32_t)sizeof(disk_catalog_entry_t);
        uint32_t entries_end = (uint32_t)sizeof(disk_catalog_header_t) + table_size;
        if (entries_end > header->bundle_size || header->blob_base_offset < entries_end || header->blob_base_offset > header->bundle_size) {
            return 0;
        }

        disk_catalog_entry_t *entries = (disk_catalog_entry_t *)(void *)(scan + off + sizeof(disk_catalog_header_t));
        uint8_t *bundle_base = scan + off;
        uint8_t *blob_base = bundle_base + header->blob_base_offset;
        uint32_t blob_region_size = header->bundle_size - header->blob_base_offset;

        for (uint16_t i = 0; i < header->count && active_count < EXEC_MAX_ACTIVE; ++i) {
            char name_buf[17];
            copy_disk_name(name_buf, entries[i].name, 17);
            const executable_header_t *base = find_builtin_by_id(entries[i].program_id);
            const char *native_name = (base && base->name && base->name[0] != '\0') ? base->name : name_buf;

            if ((entries[i].flags & CAT_FLAG_NATIVE_ENTRY) != 0u) {
                if (load_native_app(&entries[i], blob_base, blob_region_size, native_name)) {
                    continue;
                }
            }

            if (!base) {
                continue;
            }

            if ((entries[i].flags & CAT_FLAG_BUILTIN_BRIDGE) != 0u) {
                load_app_blob(&entries[i], blob_base, blob_region_size);
            }
            add_active_program(base, name_buf);
        }

        return active_count > 0 ? 1 : 0;
    }

    return 0;
}

void exec_init(void) {
    active_count = 0;
    current_program = 0;
    exec_alloc_cursor = EXEC_LOAD_BASE;
    open_count = 0;
    maximized_index = -1;
    dragging_index = -1;
    resizing_index = -1;
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
    for (uint16_t i = 0; i < EXEC_MAX_ACTIVE; ++i) {
        open_flags[i] = 0;
    }

    if (!load_catalog_from_disk()) {
        for (uint16_t i = 0; i < (uint16_t)(sizeof(fallback_registry) / sizeof(fallback_registry[0])); ++i) {
            const executable_header_t *base = fallback_registry[i]();
            if (valid_header(base)) {
                add_active_program(base, 0);
            }
        }
    }

    // Keep Program Manager functional even if catalog loading partially fails.
    ensure_core_programs_present();
}

uint16_t exec_count(void) {
    return active_count;
}

const executable_header_t *exec_get(uint16_t index) {
    if (index >= active_count) {
        return 0;
    }
    return &active_programs[index];
}

const executable_header_t *exec_current(void) {
    return current_program;
}

const char *exec_current_name(void) {
    return current_program ? current_program->name : "NONE";
}

int exec_launch(const char *name) {
    uint16_t count = active_count;
    for (uint16_t i = 0; i < count; ++i) {
        const executable_header_t *header = exec_get(i);
        if (header && streq(header->name, name)) {
            uint8_t already_open = (open_flags[i] & OPEN_FLAG_OPEN) ? 1u : 0u;
            focus_program(i);
            if (active_is_native[i]) {
                serial_write_string("[exec] native launch: ");
                serial_write_string(header->name);
                serial_write_string("\n");
            }
            if (!already_open && current_program && current_program->on_start) {
                current_program->on_start();
            }
            return 1;
        }
    }
    return 0;
}

int exec_launch_index(uint16_t index) {
    const executable_header_t *header = exec_get(index);
    if (!header) {
        return 0;
    }
    uint8_t already_open = (open_flags[index] & OPEN_FLAG_OPEN) ? 1u : 0u;
    focus_program(index);
    if (active_is_native[index]) {
        serial_write_string("[exec] native launch: ");
        serial_write_string(header->name);
        serial_write_string("\n");
    }
    if (!already_open && current_program && current_program->on_start) {
        current_program->on_start();
    }
    return 1;
}

void exec_draw_current(void) {
    if (maximized_index >= 0) {
        uint16_t idx = (uint16_t)maximized_index;
        if (idx < active_count && (open_flags[idx] & OPEN_FLAG_OPEN) && !(open_flags[idx] & OPEN_FLAG_MINIMIZED)) {
            if (active_programs[idx].on_draw) {
                int16_t base_x = 0;
                int16_t base_y = 0;
                int16_t base_w = 0;
                int16_t base_h = 0;
                get_default_window_rect(idx, &base_x, &base_y, &base_w, &base_h);
                drawing_program_index = (int16_t)idx;
                graphics_begin_window_transform(window_x[idx] - base_x, window_y[idx] - base_y, window_x[idx], window_y[idx], window_w[idx], window_h[idx], current_program == &active_programs[idx]);
                active_programs[idx].on_draw();
                graphics_end_window_transform();
                drawing_program_index = -1;
            }
            return;
        }
        maximized_index = -1;
    }

    for (uint16_t i = 0; i < open_count; ++i) {
        uint16_t idx = open_order[i];
        if (!(open_flags[idx] & OPEN_FLAG_OPEN)) {
            continue;
        }
        if (open_flags[idx] & OPEN_FLAG_MINIMIZED) {
            continue;
        }
        if (active_programs[idx].on_draw) {
            int16_t base_x = 0;
            int16_t base_y = 0;
            int16_t base_w = 0;
            int16_t base_h = 0;
            get_default_window_rect(idx, &base_x, &base_y, &base_w, &base_h);
            drawing_program_index = (int16_t)idx;
            graphics_begin_window_transform(window_x[idx] - base_x, window_y[idx] - base_y, window_x[idx], window_y[idx], window_w[idx], window_h[idx], current_program == &active_programs[idx]);
            active_programs[idx].on_draw();
            graphics_end_window_transform();
            drawing_program_index = -1;
        }
    }
}

int exec_current_window_rect(int16_t *x_out, int16_t *y_out, int16_t *w_out, int16_t *h_out) {
    int16_t idx = current_window_index_for_query();
    if (idx < 0 || (uint16_t)idx >= active_count) {
        return 0;
    }
    if (x_out) {
        *x_out = window_x[(uint16_t)idx];
    }
    if (y_out) {
        *y_out = window_y[(uint16_t)idx];
    }
    if (w_out) {
        *w_out = window_w[(uint16_t)idx];
    }
    if (h_out) {
        *h_out = window_h[(uint16_t)idx];
    }
    return 1;
}

void exec_deliver_key(uint16_t key) {
    if (current_program && current_program->on_key) {
        current_program->on_key(key);
    }
}

void exec_deliver_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button) {
    uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);

    if (resizing_index >= 0) {
        if (left_button) {
            resize_window_edges((uint16_t)resizing_index, x, y);
            prev_left_button = left_button;
            return;
        }
        resizing_index = -1;
        resizing_left_edge = 0;
        resizing_right_edge = 0;
        resizing_top_edge = 0;
        resizing_bottom_edge = 0;
    }

    if (dragging_index >= 0) {
        if (left_button) {
            move_window((uint16_t)dragging_index, (int16_t)(x - drag_offset_x), (int16_t)(y - drag_offset_y));
            prev_left_button = left_button;
            return;
        }
        dragging_index = -1;
    }

    if (left_edge) {
        if (maximized_index >= 0) {
            uint16_t idx = (uint16_t)maximized_index;
            int16_t wx = window_x[idx];
            int16_t wy = window_y[idx];
            int16_t ww = window_w[idx];
            if ((open_flags[idx] & OPEN_FLAG_OPEN) && !(open_flags[idx] & OPEN_FLAG_MINIMIZED)) {
                int16_t min_x = (int16_t)(wx + ww - 37);
                int16_t max_x = (int16_t)(wx + ww - 25);
                int16_t close_x = (int16_t)(wx + ww - 13);
                int16_t by = (int16_t)(wy + 2);

                if (y >= by && y <= by + 10) {
                    if (x >= close_x && x <= close_x + 10) {
                        close_program(idx);
                        prev_left_button = left_button;
                        return;
                    }
                    if (x >= min_x && x <= min_x + 10) {
                        open_flags[idx] |= OPEN_FLAG_MINIMIZED;
                        maximized_index = -1;
                        select_top_visible();
                        prev_left_button = left_button;
                        return;
                    }
                    if (x >= max_x && x <= max_x + 10) {
                        toggle_maximize_program(idx);
                        select_top_visible();
                        prev_left_button = left_button;
                        return;
                    }
                }
            }
        } else {
            for (int i = (int)open_count - 1; i >= 0; --i) {
                uint16_t idx = open_order[i];
                int16_t wx = window_x[idx];
                int16_t wy = window_y[idx];
                int16_t ww = window_w[idx];
                int16_t wh = window_h[idx];
                if (!(open_flags[idx] & OPEN_FLAG_OPEN) || (open_flags[idx] & OPEN_FLAG_MINIMIZED)) {
                    continue;
                }
                if (x < wx || x > wx + ww || y < wy || y > wy + wh) {
                    continue;
                }

                focus_program(idx);

                if (y >= wy + 2 && y <= wy + 12) {
                    int16_t min_x = (int16_t)(wx + ww - 37);
                    int16_t max_x = (int16_t)(wx + ww - 25);
                    int16_t close_x = (int16_t)(wx + ww - 13);
                    if (x >= close_x && x <= close_x + 10) {
                        close_program(idx);
                        prev_left_button = left_button;
                        return;
                    }
                    if (x >= min_x && x <= min_x + 10) {
                        open_flags[idx] |= OPEN_FLAG_MINIMIZED;
                        select_top_visible();
                        prev_left_button = left_button;
                        return;
                    }
                    if (x >= max_x && x <= max_x + 10) {
                        toggle_maximize_program(idx);
                        select_top_visible();
                        prev_left_button = left_button;
                        return;
                    }

                    dragging_index = (int16_t)idx;
                    drag_offset_x = (int16_t)(x - wx);
                    drag_offset_y = (int16_t)(y - wy);
                    prev_left_button = left_button;
                    return;
                }

                if (x >= wx - 2 && x <= wx + ww + 1 && y >= wy - 2 && y <= wy + wh + 1) {
                    uint8_t left_zone = (uint8_t)(x <= wx + 2);
                    uint8_t right_zone = (uint8_t)(x >= wx + ww - 3);
                    uint8_t top_zone = (uint8_t)(y <= wy + 2);
                    uint8_t bottom_zone = (uint8_t)(y >= wy + wh - 3);

                    if (left_zone || right_zone || top_zone || bottom_zone) {
                    resizing_index = (int16_t)idx;
                    resizing_left_edge = left_zone;
                    resizing_right_edge = right_zone;
                    resizing_top_edge = top_zone;
                    resizing_bottom_edge = bottom_zone;
                    resize_origin_mouse_x = x;
                    resize_origin_mouse_y = y;
                    resize_origin_x = wx;
                    resize_origin_y = wy;
                    resize_origin_w = ww;
                    resize_origin_h = wh;
                    drag_offset_x = (int16_t)(x - wx);
                    drag_offset_y = (int16_t)(y - wy);
                    prev_left_button = left_button;
                    return;
                    }
                }
                break;
            }
        }
    }

    if (current_program && current_program->on_mouse) {
        int16_t tx = x;
        int16_t ty = y;
        int16_t idx = current_program_index();
        if (idx >= 0) {
            translate_mouse_to_program((uint16_t)idx, &tx, &ty);
        }
        current_program->on_mouse(tx, ty, left_button, right_button);
    }

    prev_left_button = left_button;
}

uint16_t exec_open_window_count(void) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < open_count; ++i) {
        uint16_t idx = open_order[i];
        if (idx < active_count && (open_flags[idx] & OPEN_FLAG_OPEN)) {
            count++;
        }
    }
    return count;
}

int exec_open_window_info(uint16_t visible_index, char *name_out, uint16_t name_out_size, uint8_t *minimized_out, uint8_t *focused_out) {
    uint16_t cursor = 0;
    int16_t focused_index = current_program_index();

    if (!name_out || name_out_size == 0) {
        return 0;
    }

    for (uint16_t i = 0; i < open_count; ++i) {
        uint16_t idx = open_order[i];
        if (idx >= active_count || !(open_flags[idx] & OPEN_FLAG_OPEN)) {
            continue;
        }

        if (cursor == visible_index) {
            uint16_t n = 0;
            const char *src = active_programs[idx].name;
            while (src[n] && n + 1 < name_out_size) {
                name_out[n] = src[n];
                n++;
            }
            name_out[n] = '\0';

            if (minimized_out) {
                *minimized_out = (open_flags[idx] & OPEN_FLAG_MINIMIZED) ? 1u : 0u;
            }
            if (focused_out) {
                *focused_out = (focused_index == (int16_t)idx) ? 1u : 0u;
            }
            return 1;
        }
        cursor++;
    }

    return 0;
}

int exec_restore_window_by_visible_index(uint16_t visible_index) {
    uint16_t cursor = 0;
    for (uint16_t i = 0; i < open_count; ++i) {
        uint16_t idx = open_order[i];
        if (idx >= active_count || !(open_flags[idx] & OPEN_FLAG_OPEN)) {
            continue;
        }

        if (cursor == visible_index) {
            open_flags[idx] &= (uint8_t)~OPEN_FLAG_MINIMIZED;
            focus_program(idx);
            return 1;
        }
        cursor++;
    }

    return 0;
}

int exec_current_window_metrics(int16_t *x_out, int16_t *y_out, int16_t *w_out, int16_t *h_out) {
    int16_t idx = current_program_index();
    if (idx < 0) {
        return 0;
    }
    if (x_out) {
        *x_out = window_x[(uint16_t)idx];
    }
    if (y_out) {
        *y_out = window_y[(uint16_t)idx];
    }
    if (w_out) {
        *w_out = window_w[(uint16_t)idx];
    }
    if (h_out) {
        *h_out = window_h[(uint16_t)idx];
    }
    return 1;
}
