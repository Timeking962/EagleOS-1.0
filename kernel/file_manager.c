// EagleOS 1.0 File Manager Program.
#include <stdint.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"
#include "../include/keyboard.h"
#include "../include/exec.h"
#include "../include/fs.h"
#include "../include/system.h"

#define MAX_VIEW_FILES 24
#define MAX_VISIBLE_ROWS 11

typedef enum fm_action {
    FM_ACT_OPEN,
    FM_ACT_NEW_FILE,
    FM_ACT_NEW_FOLDER,
    FM_ACT_DELETE,
    FM_ACT_RENAME,
    FM_ACT_REFRESH,
    FM_ACT_EXIT,
    FM_ACT_ABOUT
} fm_action_t;

typedef struct fm_menu_item {
    const char *label;
    fm_action_t action;
} fm_menu_item_t;

typedef struct file_row {
    char name[FS_NAME_LEN];
    char target[FS_NAME_LEN];
    uint16_t size;
    uint8_t kind;
} file_row_t;

#define ROW_KIND_FILE 1
#define ROW_KIND_EXEC 2
#define ROW_KIND_DIR  3
#define ROW_KIND_PARENT 4

static const fm_menu_item_t FILE_MENU_ITEMS[] = {
    {"OPEN", FM_ACT_OPEN},
    {"NEW FILE", FM_ACT_NEW_FILE},
    {"NEW FOLDER", FM_ACT_NEW_FOLDER},
    {"DELETE", FM_ACT_DELETE},
    {"RENAME", FM_ACT_RENAME},
    {"REFRESH", FM_ACT_REFRESH},
    {"EXIT", FM_ACT_EXIT}
};

static const fm_menu_item_t HELP_MENU_ITEMS[] = {
    {"ABOUT", FM_ACT_ABOUT}
};

static file_row_t rows[MAX_VIEW_FILES];
static uint16_t row_count = 0;
static uint16_t selected = 0;
static uint16_t row_top = 0;
static uint8_t prev_left_button = 0;
static char status_text[32] = "READY";
static uint8_t menu_open = 0;
static uint8_t active_menu = 0;
static uint8_t active_menu_item = 0;
static uint8_t new_file_dialog_open = 0;
static uint8_t rename_dialog_open = 0;
static uint8_t folder_dialog_open = 0;
static char new_file_input[FS_NAME_LEN] = "";
static char rename_input[FS_NAME_LEN] = "";
static char folder_input[FS_NAME_LEN] = "";
static uint8_t delete_confirm_open = 0;
static uint8_t delete_confirm_yes = 1;
static char delete_target_name[FS_NAME_LEN] = "";
static uint8_t delete_target_kind = ROW_KIND_FILE;
static uint8_t about_open = 0;
static char current_path[FS_NAME_LEN] = "";
static void ensure_selection_visible(void);
static void set_status(const char *text);

static void copy_text(char *dst, const char *src, uint8_t max_len) {
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
        ++i;
    }
    dst[i] = '\0';
}

static void draw_u16(int x, int y, uint16_t value, uint8_t color) {
    char buf[6];
    int i = 0;
    if (value == 0) {
        graphics_text(x, y, "0", color);
        return;
    }
    while (value > 0 && i < 5) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    {
        char text[6];
        int o = 0;
        while (i > 0) {
            text[o++] = buf[--i];
        }
        text[o] = '\0';
        graphics_text(x, y, text, color);
    }
}

static uint8_t text_len(const char *s) {
    uint8_t n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

static uint8_t is_valid_filename_char(char ch) {
    if (ch >= 'a' && ch <= 'z') return 1;
    if (ch >= 'A' && ch <= 'Z') return 1;
    if (ch >= '0' && ch <= '9') return 1;
    if (ch == '.' || ch == '_' || ch == '-' || ch == '/') return 1;
    return 0;
}

static char to_upper(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

static void format_exec_filename(char *dst, uint8_t max_len, const char *exec_name) {
    uint8_t i = 0;
    if (!dst || max_len == 0) {
        return;
    }
    if (!exec_name) {
        dst[0] = '\0';
        return;
    }

    while (exec_name[i] && i + 5 < max_len) {
        dst[i] = exec_name[i];
        i++;
    }

    if (i + 5 < max_len) {
        dst[i++] = '.';
        dst[i++] = 'E';
        dst[i++] = 'G';
        dst[i++] = 'L';
    }
    dst[i] = '\0';
}

static uint8_t is_root_path(void) {
    return current_path[0] == '\0' ? 1u : 0u;
}

static void set_current_path(const char *path) {
    if (!path || !path[0] || (path[0] == '/' && !path[1])) {
        current_path[0] = '\0';
        return;
    }
    copy_text(current_path, path, FS_NAME_LEN);
}

static void build_child_path(char *dst, const char *name) {
    uint8_t len = 0;
    if (!dst) {
        return;
    }
    if (is_root_path()) {
        copy_text(dst, name, FS_NAME_LEN);
        return;
    }

    copy_text(dst, current_path, FS_NAME_LEN);
    while (dst[len]) {
        ++len;
    }
    if (len + 1 < FS_NAME_LEN) {
        dst[len++] = '/';
        dst[len] = '\0';
    }
    copy_text(dst + len, name, (uint8_t)(FS_NAME_LEN - len));
}

static void build_parent_path(char *dst) {
    uint8_t len = 0;
    if (!dst) {
        return;
    }
    copy_text(dst, current_path, FS_NAME_LEN);
    while (dst[len]) {
        ++len;
    }
    while (len > 0 && dst[len - 1] != '/') {
        --len;
    }
    if (len > 0) {
        dst[len - 1] = '\0';
        return;
    }
    dst[0] = '\0';
}

static void set_path_status(void) {
    set_status(is_root_path() ? "ROOT" : current_path);
}

static void draw_path_label(void) {
    graphics_text(34, 42, "PATH:", 15);
    if (is_root_path()) {
        graphics_text_small(74, 44, "ROOT", 14);
    } else {
        graphics_text_small(74, 44, current_path, 14);
    }
}

static void refresh_rows(void) {
    uint16_t fs_count = fs_list_count(current_path);
    uint16_t total_exec = exec_count();
    uint16_t out = 0;

    if (!is_root_path() && out < MAX_VIEW_FILES) {
        rows[out].kind = ROW_KIND_PARENT;
        rows[out].size = 0;
        copy_text(rows[out].name, "..", (uint8_t)sizeof(rows[out].name));
        build_parent_path(rows[out].target);
        out++;
    }

    for (uint16_t i = 0; i < fs_count && out < MAX_VIEW_FILES; ++i) {
        uint8_t kind = 0;
        rows[out].name[0] = '\0';
        rows[out].target[0] = '\0';
        rows[out].size = 0;
        if (fs_list_entry(current_path, i, rows[out].name, (uint16_t)sizeof(rows[out].name), &rows[out].size, &kind)) {
            rows[out].kind = (kind == FS_ENTRY_KIND_DIR) ? ROW_KIND_DIR : ROW_KIND_FILE;
            build_child_path(rows[out].target, rows[out].name);
            out++;
        }
    }

    if (is_root_path()) {
        for (uint16_t i = 0; i < total_exec && out < MAX_VIEW_FILES; ++i) {
            const executable_header_t *header = exec_get(i);
            if (!header || !header->name || !header->name[0]) {
                continue;
            }
            rows[out].kind = ROW_KIND_EXEC;
            rows[out].size = 0;
            copy_text(rows[out].target, header->name, (uint8_t)sizeof(rows[out].target));
            format_exec_filename(rows[out].name, (uint8_t)sizeof(rows[out].name), header->name);
            out++;
        }
    }

    row_count = out;

    if (selected >= row_count) {
        selected = 0;
    }
    ensure_selection_visible();
}

static void set_status(const char *text) {
    copy_text(status_text, text, (uint8_t)sizeof(status_text));
}

static const fm_menu_item_t *active_menu_items(uint8_t *out_count) {
    if (active_menu == 0) {
        if (out_count) {
            *out_count = (uint8_t)(sizeof(FILE_MENU_ITEMS) / sizeof(FILE_MENU_ITEMS[0]));
        }
        return FILE_MENU_ITEMS;
    }
    if (out_count) {
        *out_count = (uint8_t)(sizeof(HELP_MENU_ITEMS) / sizeof(HELP_MENU_ITEMS[0]));
    }
    return HELP_MENU_ITEMS;
}

static uint8_t menu_x(uint8_t index) {
    return (index == 0) ? 30 : 76;
}

static void ensure_selection_visible(void) {
    if (row_count == 0) {
        row_top = 0;
        return;
    }

    if (selected < row_top) {
        row_top = selected;
    }
    if (selected >= row_top + MAX_VISIBLE_ROWS) {
        row_top = (uint16_t)(selected - MAX_VISIBLE_ROWS + 1);
    }
}

static void open_menu(void) {
    menu_open = 1;
    active_menu_item = 0;
}

static void close_menu(void) {
    menu_open = 0;
}

static void open_selected_file(void) {
    if (row_count == 0 || selected >= row_count) {
        set_status("NO FILE SELECTED");
        return;
    }

    if (rows[selected].kind == ROW_KIND_PARENT || rows[selected].kind == ROW_KIND_DIR) {
        set_current_path(rows[selected].target);
        selected = 0;
        row_top = 0;
        refresh_rows();
        set_path_status();
        return;
    }

    if (rows[selected].kind == ROW_KIND_EXEC) {
        exec_launch(rows[selected].target);
        return;
    }

    fs_request_open_file(rows[selected].target);
    exec_launch("EDITOR");
}

static void start_delete_selected(void) {
    if (row_count == 0 || selected >= row_count) {
        set_status("NO FILE SELECTED");
        return;
    }

    if (rows[selected].kind == ROW_KIND_EXEC || rows[selected].kind == ROW_KIND_PARENT) {
        set_status("CANNOT DELETE ITEM");
        return;
    }

    if (fs_path_is_read_only(rows[selected].target)) {
        set_status("OS FILE READ ONLY");
        return;
    }

    copy_text(delete_target_name, rows[selected].target, (uint8_t)sizeof(delete_target_name));
    delete_target_kind = rows[selected].kind;
    delete_confirm_open = 1;
    delete_confirm_yes = 1;
    close_menu();
    set_status("CONFIRM DELETE");
}

static void confirm_delete_selected(void) {
    if (!delete_target_name[0]) {
        delete_confirm_open = 0;
        set_status("DELETE FAILED");
        return;
    }

    if ((delete_target_kind == ROW_KIND_DIR ? fs_delete_dir(delete_target_name) : fs_delete_file(delete_target_name))) {
        refresh_rows();
        set_status(delete_target_kind == ROW_KIND_DIR ? "FOLDER DELETED" : "FILE DELETED");
    } else {
        set_status("DELETE FAILED");
    }

    delete_target_name[0] = '\0';
    delete_target_kind = ROW_KIND_FILE;
    delete_confirm_open = 0;
}

static void start_create_file(void) {
    new_file_input[0] = '\0';
    new_file_dialog_open = 1;
    close_menu();
    set_status("NEW FILE");
}

static void confirm_create_file(void) {
    char file_target[FS_NAME_LEN];
    static const uint8_t empty_byte = 0;

    if (!new_file_input[0]) {
        set_status("NAME REQUIRED");
        return;
    }

    build_child_path(file_target, new_file_input);
    if (!fs_write_file(file_target, &empty_byte, 0)) {
        set_status("CREATE FAILED");
        return;
    }

    new_file_dialog_open = 0;
    refresh_rows();
    fs_request_open_file(file_target);
    exec_launch("EDITOR");
}

static void start_create_folder(void) {
    folder_input[0] = '\0';
    folder_dialog_open = 1;
    close_menu();
    set_status("NEW FOLDER");
}

static void confirm_create_folder(void) {
    char folder_target[FS_NAME_LEN];

    if (!folder_input[0]) {
        set_status("NAME REQUIRED");
        return;
    }

    build_child_path(folder_target, folder_input);
    if (fs_create_dir(folder_target)) {
        refresh_rows();
        folder_dialog_open = 0;
        set_status("FOLDER CREATED");
    } else {
        set_status("CREATE FAILED");
    }
}

static void start_rename_selected(void) {
    if (row_count == 0 || selected >= row_count) {
        set_status("NO FILE SELECTED");
        return;
    }

    if (rows[selected].kind == ROW_KIND_EXEC || rows[selected].kind == ROW_KIND_PARENT) {
        set_status("CANNOT RENAME ITEM");
        return;
    }

    if (fs_path_is_read_only(rows[selected].target)) {
        set_status("OS FILE READ ONLY");
        return;
    }

    copy_text(rename_input, rows[selected].name, FS_NAME_LEN);
    rename_dialog_open = 1;
    close_menu();
    set_status("RENAME FILE");
}

static void confirm_rename(void) {
    char new_target[FS_NAME_LEN];
    if (row_count == 0 || selected >= row_count) {
        rename_dialog_open = 0;
        set_status("NO FILE SELECTED");
        return;
    }

    if (!rename_input[0]) {
        set_status("NAME REQUIRED");
        return;
    }

    build_child_path(new_target, rename_input);
    if ((rows[selected].kind == ROW_KIND_DIR ? fs_rename_dir(rows[selected].target, new_target) : fs_rename_file(rows[selected].target, new_target))) {
        refresh_rows();
        set_status(rows[selected].kind == ROW_KIND_DIR ? "FOLDER RENAMED" : "FILE RENAMED");
        rename_dialog_open = 0;
    } else {
        set_status("RENAME FAILED");
    }
}

static void perform_action(fm_action_t action) {
    if (action == FM_ACT_OPEN) {
        open_selected_file();
    } else if (action == FM_ACT_NEW_FILE) {
        start_create_file();
    } else if (action == FM_ACT_NEW_FOLDER) {
        start_create_folder();
    } else if (action == FM_ACT_DELETE) {
        start_delete_selected();
    } else if (action == FM_ACT_RENAME) {
        start_rename_selected();
    } else if (action == FM_ACT_REFRESH) {
        refresh_rows();
        set_status("REFRESHED");
    } else if (action == FM_ACT_EXIT) {
        exec_launch("PROGMAN");
    } else if (action == FM_ACT_ABOUT) {
        about_open = 1;
        close_menu();
    }
}

static void draw_about_dialog(void) {
    if (!about_open) {
        return;
    }

    dim_draw_about_dialog(62, 54, 192, "ABOUT FILEMAN", "FILEMAN");
}

static void draw_delete_confirm(void) {
    if (!delete_confirm_open) {
        return;
    }
    dim_draw_confirm_dialog(72, 70, 176, "CONFIRM DELETE", (delete_target_kind == ROW_KIND_DIR) ? "DELETE EMPTY FOLDER?" : "DELETE SELECTED FILE?", "YES", "NO", delete_confirm_yes != 0);
}

static void fm_start(void) {
    selected = 0;
    prev_left_button = 0;
    menu_open = 0;
    active_menu = 0;
    active_menu_item = 0;
    new_file_dialog_open = 0;
    rename_dialog_open = 0;
    delete_confirm_open = 0;
    delete_confirm_yes = 1;
    delete_target_name[0] = '\0';
    delete_target_kind = ROW_KIND_FILE;
    about_open = 0;
    row_top = 0;
    current_path[0] = '\0';
    new_file_input[0] = '\0';
    rename_input[0] = '\0';
    folder_input[0] = '\0';
    folder_dialog_open = 0;
    set_status("READY");
    refresh_rows();
}

static void draw_menu_bar(void) {
    for (uint8_t i = 0; i < 2; ++i) {
        uint8_t selected = (uint8_t)(menu_open && i == active_menu);
        dim_draw_menu_title(menu_x(i), 29, 42, (i == 0) ? "FILE" : "HELP", selected);
    }

    graphics_text(220, 29, fs_persistence_active() ? "DISK" : "SESSION", 14);
}

static void draw_menu_dropdown(void) {
    if (!menu_open) {
        return;
    }

    uint8_t menu_count = 0;
    const fm_menu_item_t *items = active_menu_items(&menu_count);

    int x = menu_x(active_menu) - 4;
    int y = 38;
    int w = 104;
    int h = menu_count * 12 + 2;

    dim_draw_menu_dropdown_frame(x, y, w, h);

    for (uint8_t i = 0; i < menu_count; ++i) {
        int iy = y + 2 + i * 12;
        dim_draw_menu_dropdown_item(x, iy, w, items[i].label, i == active_menu_item);
    }
}

static void draw_footer(void) {
    char position_text[6];
    position_text[0] = '\0';

    if (row_count > MAX_VISIBLE_ROWS) {
        uint16_t value = (uint16_t)(selected + 1);
        uint8_t pos = 0;
        char digits[6];
        if (value == 0) {
            digits[pos++] = '0';
        } else {
            while (value > 0 && pos < sizeof(digits) - 1) {
                digits[pos++] = (char)('0' + (value % 10));
                value /= 10;
            }
        }
        for (uint8_t i = 0; i < pos; ++i) {
            position_text[i] = digits[pos - 1 - i];
        }
        position_text[pos] = '\0';
    }

    dim_draw_file_manager_footer(28, 166, 262, "ENTER OPEN W FILE F FOLDER", status_text, position_text);
}

static void draw_new_file_dialog(void) {
    if (!new_file_dialog_open) {
        return;
    }
    dim_draw_input_dialog(72, 70, 176, "NEW FILE", "FILENAME:", new_file_input, "CREATE", "CANCEL");
}

static void draw_rename_dialog(void) {
    if (!rename_dialog_open) {
        return;
    }
    dim_draw_input_dialog(72, 70, 176, "RENAME FILE", "NEW NAME:", rename_input, "RENAME", "CANCEL");
}

static void draw_folder_dialog(void) {
    if (!folder_dialog_open) {
        return;
    }
    dim_draw_input_dialog(72, 70, 176, "NEW FOLDER", "FOLDER NAME:", folder_input, "CREATE", "CANCEL");
}

static void fm_draw(void) {
    int16_t win_x = 20;
    int16_t win_y = 14;
    int16_t win_w = 280;
    int16_t win_h = 172;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int footer_y;
    int footer_w;
    int path_x;
    int path_value_x;

    (void)exec_current_window_rect(&win_x, &win_y, &win_w, &win_h);

    content_x = win_x + 8;
    content_y = win_y + 38;
    content_w = win_w - 18;
    content_h = win_h - 60;
    footer_y = win_y + win_h - 20;
    footer_w = win_w - 18;
    path_x = win_x + 14;
    path_value_x = win_x + 54;

    dim_draw_app_window(win_x, win_y, win_w, win_h, "FILE MANAGER", win_x + 1, win_y + 12, win_w - 2, true);

    draw_menu_bar();
    graphics_text(path_x, win_y + 28, "PATH:", 15);
    if (is_root_path()) {
        graphics_text_small(path_value_x, win_y + 30, "ROOT", 14);
    } else {
        graphics_text_small(path_value_x, win_y + 30, current_path, 14);
    }

    graphics_rect(content_x, content_y, content_w, content_h, 0);
    graphics_box(content_x, content_y, content_w, content_h, 15);

    graphics_text(content_x + 6, content_y + 4, "NAME", 15);
    graphics_text(content_x + content_w - 68, content_y + 4, "SIZE", 15);

    int y = content_y + 16;
    uint16_t end = row_count;
    if (end > row_top + MAX_VISIBLE_ROWS) {
        end = (uint16_t)(row_top + MAX_VISIBLE_ROWS);
    }
    for (uint16_t i = row_top; i < end; ++i) {
        if (i == selected) {
            graphics_rect(content_x + 2, y - 1, content_w - 6, 9, 8);
        }
        graphics_text(content_x + 6, y, rows[i].name, 15);
        if (rows[i].kind == ROW_KIND_EXEC) {
            graphics_text(content_x + content_w - 60, y, "EGL", 14);
        } else if (rows[i].kind == ROW_KIND_DIR || rows[i].kind == ROW_KIND_PARENT) {
            graphics_text(content_x + content_w - 76, y, "<DIR>", 14);
        } else {
            draw_u16(content_x + content_w - 60, y, rows[i].size, 14);
        }
        y += 10;
    }

    if (row_count == 0) {
        graphics_text(content_x + 6, content_y + 16, is_root_path() ? "NO FILES" : "EMPTY FOLDER", 8);
    }

    {
        char position_text[6];
        position_text[0] = '\0';

        if (row_count > MAX_VISIBLE_ROWS) {
            uint16_t value = (uint16_t)(selected + 1);
            uint8_t pos = 0;
            char digits[6];
            if (value == 0) {
                digits[pos++] = '0';
            } else {
                while (value > 0 && pos < sizeof(digits) - 1) {
                    digits[pos++] = (char)('0' + (value % 10));
                    value /= 10;
                }
            }
            for (uint8_t i = 0; i < pos; ++i) {
                position_text[i] = digits[pos - 1 - i];
            }
            position_text[pos] = '\0';
        }

        dim_draw_file_manager_footer(content_x, footer_y, footer_w, "ENTER OPEN W FILE F FOLDER", status_text, position_text);
    }

    draw_menu_dropdown();
    draw_new_file_dialog();
    draw_rename_dialog();
    draw_folder_dialog();
    draw_delete_confirm();
    draw_about_dialog();
}

static void fm_key(uint16_t key) {
    uint8_t menu_count = 0;
    const fm_menu_item_t *items = active_menu_items(&menu_count);

    if (about_open) {
        if (key == KEY_ENTER || key == KEY_ESCAPE) {
            about_open = 0;
        }
        return;
    }

    if (delete_confirm_open) {
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            delete_confirm_yes = (uint8_t)!delete_confirm_yes;
            return;
        }
        if (key == KEY_ENTER) {
            if (delete_confirm_yes) {
                confirm_delete_selected();
            } else {
                delete_confirm_open = 0;
                delete_target_name[0] = '\0';
                set_status("DELETE CANCELED");
            }
            return;
        }
        if (key == KEY_ESCAPE) {
            delete_confirm_open = 0;
            delete_target_name[0] = '\0';
            set_status("DELETE CANCELED");
            return;
        }
        return;
    }

    if (new_file_dialog_open) {
        if (key == KEY_ENTER) {
            confirm_create_file();
            return;
        }
        if (key == KEY_ESCAPE) {
            new_file_dialog_open = 0;
            set_status("CREATE CANCELED");
            return;
        }
        if (key == KEY_BACKSPACE) {
            uint8_t len = text_len(new_file_input);
            if (len > 0) {
                new_file_input[len - 1] = '\0';
            }
            return;
        }
        if (KEY_IS_CHAR(key)) {
            char ch = KEY_TO_CHAR(key);
            uint8_t len = text_len(new_file_input);
            if (!is_valid_filename_char(ch)) {
                return;
            }
            if (len + 1 >= FS_NAME_LEN) {
                set_status("NAME TOO LONG");
                return;
            }
            new_file_input[len] = to_upper(ch);
            new_file_input[len + 1] = '\0';
        }
        return;
    }

    if (folder_dialog_open) {
        if (key == KEY_ENTER) {
            confirm_create_folder();
            return;
        }
        if (key == KEY_ESCAPE) {
            folder_dialog_open = 0;
            set_status("CREATE CANCELED");
            return;
        }
        if (key == KEY_BACKSPACE) {
            uint8_t len = text_len(folder_input);
            if (len > 0) {
                folder_input[len - 1] = '\0';
            }
            return;
        }
        if (KEY_IS_CHAR(key)) {
            char ch = KEY_TO_CHAR(key);
            uint8_t len = text_len(folder_input);
            if (!is_valid_filename_char(ch)) {
                return;
            }
            if (len + 1 >= FS_NAME_LEN) {
                set_status("NAME TOO LONG");
                return;
            }
            folder_input[len] = to_upper(ch);
            folder_input[len + 1] = '\0';
        }
        return;
    }

    if (rename_dialog_open) {
        if (key == KEY_ENTER) {
            confirm_rename();
            return;
        }
        if (key == KEY_ESCAPE) {
            rename_dialog_open = 0;
            set_status("RENAME CANCELED");
            return;
        }
        if (key == KEY_BACKSPACE) {
            uint8_t len = text_len(rename_input);
            if (len > 0) {
                rename_input[len - 1] = '\0';
            }
            return;
        }
        if (KEY_IS_CHAR(key)) {
            char ch = KEY_TO_CHAR(key);
            uint8_t len = text_len(rename_input);
            if (!is_valid_filename_char(ch)) {
                return;
            }
            if (len + 1 >= FS_NAME_LEN) {
                set_status("NAME TOO LONG");
                return;
            }
            rename_input[len] = to_upper(ch);
            rename_input[len + 1] = '\0';
        }
        return;
    }

    if (key == KEY_TAB) {
        return;
    }

    if (key == KEY_ESCAPE) {
        if (menu_open) {
            close_menu();
        } else if (!is_root_path()) {
            char parent_path[FS_NAME_LEN];
            build_parent_path(parent_path);
            set_current_path(parent_path);
            selected = 0;
            row_top = 0;
            refresh_rows();
            set_path_status();
        } else {
            exec_launch("PROGMAN");
        }
        return;
    }

    if (menu_open) {
        if (key == KEY_UP) {
            active_menu_item = (uint8_t)((active_menu_item + menu_count - 1) % menu_count);
        } else if (key == KEY_DOWN) {
            active_menu_item = (uint8_t)((active_menu_item + 1) % menu_count);
        } else if (key == KEY_LEFT) {
            active_menu = (uint8_t)((active_menu + 1) % 2);
            active_menu_item = 0;
            items = active_menu_items(&menu_count);
            (void)items;
        } else if (key == KEY_RIGHT) {
            active_menu = (uint8_t)((active_menu + 1) % 2);
            active_menu_item = 0;
            items = active_menu_items(&menu_count);
            (void)items;
        } else if (key == KEY_ENTER) {
            perform_action(items[active_menu_item].action);
            if (!rename_dialog_open) {
                close_menu();
            }
        }
        return;
    }

    if (key == KEY_UP) {
        if (selected > 0) {
            selected--;
            ensure_selection_visible();
        }
    } else if (key == KEY_DOWN) {
        if (selected + 1 < row_count) {
            selected++;
            ensure_selection_visible();
        }
    } else if (key == KEY_ENTER) {
        open_selected_file();
    } else if (KEY_IS_CHAR(key)) {
        char ch = KEY_TO_CHAR(key);
        if (ch == 'w' || ch == 'W') {
            start_create_file();
        } else if (ch == 'f' || ch == 'F') {
            start_create_folder();
        } else if (ch == 'r' || ch == 'R') {
            refresh_rows();
            set_status("REFRESHED");
        } else if (ch == 'd' || ch == 'D') {
            start_delete_selected();
        } else if (ch == 'n' || ch == 'N') {
            start_rename_selected();
        } else if (ch == 'm' || ch == 'M') {
            open_menu();
        }
    }
}

static void fm_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button) {
    uint8_t menu_count = 0;
    const fm_menu_item_t *items = active_menu_items(&menu_count);

    if (about_open) {
        uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_edge) {
            about_open = 0;
        }
        prev_left_button = left_button;
        return;
    }

    if (delete_confirm_open) {
        uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_edge) {
            if (x >= 88 && x <= 140 && y >= 114 && y <= 126) {
                confirm_delete_selected();
            } else if (x >= 162 && x <= 214 && y >= 114 && y <= 126) {
                delete_confirm_open = 0;
                delete_target_name[0] = '\0';
                set_status("DELETE CANCELED");
            }
        }
        prev_left_button = left_button;
        return;
    }

    if (new_file_dialog_open) {
        uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_edge) {
            if (x >= 88 && x <= 150 && y >= 114 && y <= 126) {
                confirm_create_file();
            } else if (x >= 162 && x <= 224 && y >= 114 && y <= 126) {
                new_file_dialog_open = 0;
                set_status("CREATE CANCELED");
            }
        }
        prev_left_button = left_button;
        return;
    }

    if (rename_dialog_open) {
        uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_edge) {
            if (x >= 88 && x <= 150 && y >= 114 && y <= 126) {
                confirm_rename();
            } else if (x >= 162 && x <= 224 && y >= 114 && y <= 126) {
                rename_dialog_open = 0;
                set_status("RENAME CANCELED");
            }
        }
        prev_left_button = left_button;
        return;
    }

    if (folder_dialog_open) {
        uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_edge) {
            if (x >= 88 && x <= 150 && y >= 114 && y <= 126) {
                confirm_create_folder();
            } else if (x >= 162 && x <= 224 && y >= 114 && y <= 126) {
                folder_dialog_open = 0;
                set_status("CREATE CANCELED");
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

    {
        uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
        if (left_edge) {
            if (y >= 26 && y <= 38 && x >= 21 && x <= 299) {
                if (x >= menu_x(0) - 4 && x <= menu_x(0) + 38) {
                    active_menu = 0;
                    open_menu();
                } else if (x >= menu_x(1) - 4 && x <= menu_x(1) + 38) {
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
                int dw = 104;
                int dh = menu_count * 12 + 2;
                if (x >= dx && x <= dx + dw && y >= dy && y <= dy + dh) {
                    int rel = y - (dy + 2);
                    if (rel >= 0) {
                        uint8_t row = (uint8_t)(rel / 12);
                        if (row < menu_count) {
                            active_menu_item = row;
                            perform_action(items[active_menu_item].action);
                            if (!rename_dialog_open) {
                                close_menu();
                            }
                            prev_left_button = left_button;
                            return;
                        }
                    }
                }
                close_menu();
            }

            if (x >= 30 && x <= 286 && y >= 67 && y < (int16_t)(67 + MAX_VISIBLE_ROWS * 10)) {
                uint16_t row = (uint16_t)((y - 67) / 10);
                uint16_t actual = (uint16_t)(row_top + row);
                if (actual < row_count) {
                    uint16_t previous = selected;
                    selected = actual;
                    ensure_selection_visible();
                    if (actual == previous) {
                        open_selected_file();
                        prev_left_button = left_button;
                        return;
                    }
                }
            }
        }
    }

    prev_left_button = left_button;
}

static const executable_header_t FILEMAN_EXECUTABLE = {
    EEXE_MAGIC,
    EEXE_VERSION,
    "FILEMAN",
    fm_start,
    fm_draw,
    fm_key,
    fm_mouse
};

const executable_header_t *file_manager_executable(void) {
    return &FILEMAN_EXECUTABLE;
}
