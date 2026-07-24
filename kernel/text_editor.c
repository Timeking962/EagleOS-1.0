// EagleOS 1.0 Text Editor Program.
#include <stdint.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"
#include "../include/keyboard.h"
#include "../include/exec.h"
#include "../include/fs.h"
#include "../include/system.h"

typedef enum editor_action {
    ACT_FILE_NEW,
    ACT_FILE_OPEN,
    ACT_FILE_SAVE,
    ACT_FILE_EXIT,
    ACT_EDIT_INSERT_SAMPLE,
    ACT_EDIT_DELETE_LAST,
    ACT_EDIT_CLEAR,
    ACT_VIEW_TOGGLE_STATUS,
    ACT_VIEW_TOGGLE_WRAP,
    ACT_HELP_ABOUT
} editor_action_t;

typedef struct menu_item {
    const char *label;
    editor_action_t action;
} menu_item_t;

typedef struct top_menu {
    const char *name;
    const menu_item_t *items;
    uint8_t item_count;
} top_menu_t;

typedef enum file_dialog_mode {
    FILE_DIALOG_NONE,
    FILE_DIALOG_SAVE,
    FILE_DIALOG_OPEN
} file_dialog_mode_t;

static const char *const samples[] = {
    "HELLO FROM EAGLEOS.",
    "THIS DOCUMENT LOOKS LIKE NOTEPAD.",
    "TYPE WITH KEYBOARD. USE MENUS VIA MOUSE.",
    "FILE MENU SUPPORTS SAVE/OPEN SLOTS."
};

static const uint8_t sample_count = 4;
static uint8_t sample_index = 0;

#define MAX_LINES 12
#define LINE_LEN  40
#define DOC_BUFFER_SIZE (MAX_LINES * (LINE_LEN + 1))
#define DEFAULT_EDITOR_FILENAME "NOTE.TXT"

static char lines[MAX_LINES][LINE_LEN + 1];
static uint8_t line_count = 1;
static uint8_t cursor_line = 0;
static uint8_t cursor_col = 0;

static uint8_t active_menu = 0;
static uint8_t menu_open = 0;
static uint8_t active_item = 0;

static uint8_t word_wrap = 0;
static uint8_t status_visible = 1;
static uint8_t prev_left_button = 0;
static uint8_t about_open = 0;
static file_dialog_mode_t file_dialog_mode = FILE_DIALOG_NONE;

static char status_text[48] = "READY";
static char current_filename[FS_NAME_LEN] = DEFAULT_EDITOR_FILENAME;
static char file_name_input[FS_NAME_LEN] = DEFAULT_EDITOR_FILENAME;

static const menu_item_t FILE_ITEMS[] = {
    {"NEW", ACT_FILE_NEW},
    {"OPEN", ACT_FILE_OPEN},
    {"SAVE", ACT_FILE_SAVE},
    {"EXIT", ACT_FILE_EXIT}
};

static const menu_item_t EDIT_ITEMS[] = {
    {"INSERT SAMPLE", ACT_EDIT_INSERT_SAMPLE},
    {"DELETE LAST", ACT_EDIT_DELETE_LAST},
    {"CLEAR", ACT_EDIT_CLEAR}
};

static const menu_item_t VIEW_ITEMS[] = {
    {"TOGGLE STATUS", ACT_VIEW_TOGGLE_STATUS},
    {"TOGGLE WRAP", ACT_VIEW_TOGGLE_WRAP}
};

static const menu_item_t HELP_ITEMS[] = {
    {"ABOUT", ACT_HELP_ABOUT}
};

static const top_menu_t MENUS[] = {
    {"FILE", FILE_ITEMS, 4},
    {"EDIT", EDIT_ITEMS, 3},
    {"VIEW", VIEW_ITEMS, 2},
    {"HELP", HELP_ITEMS, 1}
};

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

static uint8_t text_len(const char *s) {
    uint8_t n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

static uint8_t is_valid_filename_char(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return 1;
    }
    if (ch >= 'A' && ch <= 'Z') {
        return 1;
    }
    if (ch >= '0' && ch <= '9') {
        return 1;
    }
    if (ch == '.' || ch == '_' || ch == '-' || ch == '/') {
        return 1;
    }
    return 0;
}

static char to_upper(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

static void set_status(const char *text) {
    copy_text(status_text, text, (uint8_t)sizeof(status_text));
}

static uint8_t menu_x(uint8_t index) {
    if (index == 0) return 30;
    if (index == 1) return 70;
    if (index == 2) return 110;
    return 150;
}

static void clamp_cursor(void) {
    if (cursor_line >= line_count) {
        cursor_line = (line_count == 0) ? 0 : (uint8_t)(line_count - 1);
    }
    if (cursor_line >= MAX_LINES) {
        cursor_line = MAX_LINES - 1;
    }

    {
        uint8_t len = text_len(lines[cursor_line]);
        if (cursor_col > len) {
            cursor_col = len;
        }
    }
}

static void clear_document(void) {
    line_count = 1;
    lines[0][0] = '\0';
    for (uint8_t i = 1; i < MAX_LINES; ++i) {
        lines[i][0] = '\0';
    }
    cursor_line = 0;
    cursor_col = 0;
}

static void append_line(const char *text) {
    if (line_count >= MAX_LINES) {
        set_status("DOCUMENT FULL");
        return;
    }
    copy_text(lines[line_count], text, LINE_LEN + 1);
    cursor_line = line_count;
    cursor_col = text_len(lines[line_count]);
    line_count++;
}

static void remove_last_line(void) {
    if (line_count == 0) {
        set_status("NOTHING TO DELETE");
        return;
    }
    if (line_count == 1) {
        lines[0][0] = '\0';
        cursor_line = 0;
        cursor_col = 0;
        set_status("LAST LINE REMOVED");
        return;
    }

    line_count--;
    lines[line_count][0] = '\0';
    if (cursor_line >= line_count) {
        cursor_line = (uint8_t)(line_count - 1);
    }
    clamp_cursor();
}

static uint16_t serialize_document(uint8_t *out, uint16_t max_size) {
    uint16_t pos = 0;
    if (!out || max_size == 0) {
        return 0;
    }

    for (uint8_t i = 0; i < line_count; ++i) {
        uint8_t j = 0;
        while (lines[i][j]) {
            if (pos >= max_size) {
                return pos;
            }
            out[pos++] = (uint8_t)lines[i][j++];
        }
        if (i + 1 < line_count) {
            if (pos >= max_size) {
                return pos;
            }
            out[pos++] = (uint8_t)'\n';
        }
    }

    return pos;
}

static void deserialize_document(const uint8_t *data, uint16_t size) {
    uint8_t line = 0;
    uint8_t col = 0;

    clear_document();

    if (!data || size == 0) {
        return;
    }

    lines[0][0] = '\0';
    line_count = 1;

    for (uint16_t i = 0; i < size; ++i) {
        char ch = (char)data[i];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            if (line + 1 >= MAX_LINES) {
                break;
            }
            lines[line][col] = '\0';
            line++;
            col = 0;
            lines[line][0] = '\0';
            line_count = (uint8_t)(line + 1);
            continue;
        }
        if (ch < 32 || ch > 126) {
            continue;
        }
        if (col >= LINE_LEN) {
            continue;
        }
        lines[line][col++] = ch;
        lines[line][col] = '\0';
    }

    cursor_line = 0;
    cursor_col = 0;
}

static int save_file_named(const char *name) {
    uint8_t buffer[DOC_BUFFER_SIZE];
    uint16_t size = serialize_document(buffer, (uint16_t)sizeof(buffer));
    if (!name || !name[0]) {
        set_status("NAME REQUIRED");
        return 0;
    }
    if (!fs_write_file(name, buffer, size)) {
        set_status("SAVE FAILED");
        return 0;
    }
    copy_text(current_filename, name, FS_NAME_LEN);
    if (fs_persistence_active()) {
        set_status("SAVED");
    } else {
        set_status("SAVED (SESSION ONLY)");
    }
    return 1;
}

static int open_file_named(const char *name) {
    uint8_t buffer[DOC_BUFFER_SIZE];
    uint16_t size = 0;
    if (!name || !name[0]) {
        set_status("NAME REQUIRED");
        return 0;
    }
    if (!fs_read_file(name, buffer, (uint16_t)sizeof(buffer), &size)) {
        set_status("FILE NOT FOUND");
        return 0;
    }
    deserialize_document(buffer, size);
    copy_text(current_filename, name, FS_NAME_LEN);
    if (fs_persistence_active()) {
        set_status("OPENED");
    } else {
        set_status("OPENED (SESSION COPY)");
    }
    return 1;
}

static void open_save_dialog(void) {
    copy_text(file_name_input, current_filename, FS_NAME_LEN);
    file_dialog_mode = FILE_DIALOG_SAVE;
    set_status("SAVE AS");
}

static void open_open_dialog(void) {
    copy_text(file_name_input, current_filename, FS_NAME_LEN);
    file_dialog_mode = FILE_DIALOG_OPEN;
    set_status("OPEN FILE");
}

static void close_file_dialog(void) {
    file_dialog_mode = FILE_DIALOG_NONE;
}

static void confirm_file_dialog(void) {
    if (file_name_input[0] == '\0') {
        set_status("NAME REQUIRED");
        return;
    }

    if (file_dialog_mode == FILE_DIALOG_SAVE) {
        if (save_file_named(file_name_input)) {
            close_file_dialog();
        }
        return;
    }

    if (file_dialog_mode == FILE_DIALOG_OPEN) {
        if (open_file_named(file_name_input)) {
            close_file_dialog();
        }
    }
}

static void insert_char(char ch) {
    if (ch < 32 || ch > 126) {
        return;
    }

    {
        char *line = lines[cursor_line];
        uint8_t len = text_len(line);
        if (len >= LINE_LEN) {
            set_status("LINE FULL");
            return;
        }

        for (int i = len; i >= (int)cursor_col; --i) {
            line[i + 1] = line[i];
        }
        line[cursor_col] = ch;
        cursor_col++;
    }
}

static void newline_at_cursor(void) {
    if (line_count >= MAX_LINES) {
        set_status("DOCUMENT FULL");
        return;
    }

    {
        char *line = lines[cursor_line];
        uint8_t len = text_len(line);
        if (cursor_col > len) {
            cursor_col = len;
        }

        for (int i = line_count; i > (int)cursor_line + 1; --i) {
            copy_text(lines[i], lines[i - 1], LINE_LEN + 1);
        }

        copy_text(lines[cursor_line + 1], &line[cursor_col], LINE_LEN + 1);
        line[cursor_col] = '\0';

        line_count++;
        cursor_line++;
        cursor_col = 0;
    }
}

static void backspace_at_cursor(void) {
    if (cursor_col > 0) {
        char *line = lines[cursor_line];
        uint8_t len = text_len(line);
        for (uint8_t i = cursor_col; i <= len; ++i) {
            line[i - 1] = line[i];
        }
        cursor_col--;
        return;
    }

    if (cursor_line == 0) {
        return;
    }

    {
        uint8_t prev = (uint8_t)(cursor_line - 1);
        uint8_t prev_len = text_len(lines[prev]);
        uint8_t cur_len = text_len(lines[cursor_line]);

        if (prev_len + cur_len > LINE_LEN) {
            set_status("LINE FULL");
            return;
        }

        for (uint8_t i = 0; i <= cur_len; ++i) {
            lines[prev][prev_len + i] = lines[cursor_line][i];
        }

        for (uint8_t i = cursor_line; i + 1 < line_count; ++i) {
            copy_text(lines[i], lines[i + 1], LINE_LEN + 1);
        }

        line_count--;
        lines[line_count][0] = '\0';
        cursor_line = prev;
        cursor_col = prev_len;
    }
}

static void perform_action(editor_action_t action) {
    if (action == ACT_FILE_NEW) {
        clear_document();
        set_status("NEW DOCUMENT");
    } else if (action == ACT_FILE_OPEN) {
        open_open_dialog();
    } else if (action == ACT_FILE_SAVE) {
        open_save_dialog();
    } else if (action == ACT_FILE_EXIT) {
        exec_launch("PROGMAN");
    } else if (action == ACT_EDIT_INSERT_SAMPLE) {
        append_line(samples[sample_index]);
        sample_index = (uint8_t)((sample_index + 1) % sample_count);
        set_status("SAMPLE INSERTED");
    } else if (action == ACT_EDIT_DELETE_LAST) {
        remove_last_line();
        set_status("LAST LINE REMOVED");
    } else if (action == ACT_EDIT_CLEAR) {
        clear_document();
        set_status("DOCUMENT CLEARED");
    } else if (action == ACT_VIEW_TOGGLE_STATUS) {
        status_visible = (uint8_t)!status_visible;
        set_status(status_visible ? "STATUS ON" : "STATUS OFF");
    } else if (action == ACT_VIEW_TOGGLE_WRAP) {
        word_wrap = (uint8_t)!word_wrap;
        set_status(word_wrap ? "WRAP ON" : "WRAP OFF");
    } else if (action == ACT_HELP_ABOUT) {
        about_open = 1;
    }
}

static void draw_menu_bar(void) {
    for (uint8_t i = 0; i < 4; ++i) {
        uint8_t selected = (uint8_t)(i == active_menu);
        dim_draw_menu_title(menu_x(i), 25, 34, MENUS[i].name, selected);
    }
}

static void draw_dropdown(void) {
    if (!menu_open) {
        return;
    }

    {
        const top_menu_t *menu = &MENUS[active_menu];
        int x = menu_x(active_menu) - 4;
        int y = 36;
        int w = 112;
        int h = menu->item_count * 12 + 2;

        dim_draw_menu_dropdown_frame(x, y, w, h);

        for (uint8_t i = 0; i < menu->item_count; ++i) {
            int iy = y + 2 + i * 12;
            dim_draw_menu_dropdown_item(x, iy, w, menu->items[i].label, i == active_item);
        }
    }
}

static void draw_document(void) {
    int16_t win_x = 16;
    int16_t win_y = 10;
    int16_t win_w = 288;
    int16_t win_h = 176;
    int doc_x;
    int doc_y;
    int doc_w;
    int doc_h;

    (void)exec_current_window_rect(&win_x, &win_y, &win_w, &win_h);
    doc_x = win_x + 8;
    doc_y = win_y + 40;
    doc_w = win_w - 16;
    doc_h = win_h - 64;

    graphics_rect(doc_x, doc_y, doc_w, doc_h, 0);
    graphics_box(doc_x, doc_y, doc_w, doc_h, 15);

    {
        int y = doc_y + 6;
        int max_y = doc_y + doc_h - 8;
        for (uint8_t i = 0; i < line_count; ++i) {
            graphics_text(doc_x + 6, y, lines[i], 10);
            y += 9;
            if (y > max_y) {
                break;
            }
        }
    }

    {
        int cx = doc_x + 6 + ((int)cursor_col * 8);
        int cy = doc_y + 6 + ((int)cursor_line * 9);
        if (cx < doc_x + doc_w - 4 && cy < doc_y + doc_h - 4) {
            graphics_rect(cx, cy + 8, 6, 1, 14);
        }
    }
}

static void draw_status_bar(void) {
    if (!status_visible) {
        return;
    }
    {
        int16_t win_x = 16;
        int16_t win_y = 10;
        int16_t win_w = 288;
        int16_t win_h = 176;
        (void)exec_current_window_rect(&win_x, &win_y, &win_w, &win_h);
        dim_draw_editor_status_bar(win_x + 8, win_y + win_h - 12, win_w - 16, status_text, current_filename, word_wrap != 0);
    }
}

static void draw_file_dialog(void) {
    if (file_dialog_mode == FILE_DIALOG_NONE) {
        return;
    }
    dim_draw_input_dialog(
        72,
        70,
        176,
        (file_dialog_mode == FILE_DIALOG_SAVE) ? "SAVE FILE AS" : "OPEN FILE",
        "FILENAME:",
        file_name_input,
        (file_dialog_mode == FILE_DIALOG_SAVE) ? "SAVE" : "OPEN",
        "CANCEL"
    );
}

static void draw_about_dialog(void) {
    if (!about_open) {
        return;
    }
    dim_draw_about_dialog(60, 54, 192, "ABOUT EDITOR", "EDITOR");
}

static void editor_start(void) {
    uint8_t requested_open = 0;

    sample_index = 0;
    active_menu = 0;
    menu_open = 0;
    active_item = 0;
    word_wrap = 0;
    status_visible = 1;
    prev_left_button = 0;
    about_open = 0;
    file_dialog_mode = FILE_DIALOG_NONE;
    copy_text(current_filename, DEFAULT_EDITOR_FILENAME, FS_NAME_LEN);
    copy_text(file_name_input, DEFAULT_EDITOR_FILENAME, FS_NAME_LEN);
    clear_document();

    if (fs_take_open_request(current_filename, FS_NAME_LEN)) {
        requested_open = 1;
        if (!open_file_named(current_filename)) {
            clear_document();
        }
    }

    if (!requested_open) {
        set_status("READY");
    }
}

static void editor_draw(void) {
    int16_t win_x = 16;
    int16_t win_y = 10;
    int16_t win_w = 288;
    int16_t win_h = 176;

    (void)exec_current_window_rect(&win_x, &win_y, &win_w, &win_h);
    dim_draw_app_window(win_x, win_y, win_w, win_h, "TEXT EDITOR", win_x + 1, win_y + 12, win_w - 2, true);

    draw_menu_bar();
    draw_document();
    draw_status_bar();
    draw_dropdown();
    draw_file_dialog();
    draw_about_dialog();
}

static void open_menu(void) {
    menu_open = 1;
    active_item = 0;
}

static void close_menu(void) {
    menu_open = 0;
}

static void activate_current_item(void) {
    const top_menu_t *menu = &MENUS[active_menu];
    if (active_item >= menu->item_count) {
        active_item = 0;
    }
    perform_action(menu->items[active_item].action);
    close_menu();
}

static void editor_key(uint16_t key) {
    if (about_open) {
        if (key == KEY_ENTER || key == KEY_ESCAPE) {
            about_open = 0;
        }
        return;
    }

    if (file_dialog_mode != FILE_DIALOG_NONE) {
        if (key == KEY_ENTER) {
            confirm_file_dialog();
            return;
        }
        if (key == KEY_ESCAPE) {
            uint8_t was_save = (file_dialog_mode == FILE_DIALOG_SAVE) ? 1 : 0;
            close_file_dialog();
            set_status(was_save ? "SAVE CANCELED" : "OPEN CANCELED");
            return;
        }
        if (key == KEY_BACKSPACE) {
            uint8_t len = text_len(file_name_input);
            if (len > 0) {
                file_name_input[len - 1] = '\0';
            }
            return;
        }
        if (KEY_IS_CHAR(key)) {
            char ch = KEY_TO_CHAR(key);
            uint8_t len = text_len(file_name_input);
            if (!is_valid_filename_char(ch)) {
                return;
            }
            if (len + 1 >= FS_NAME_LEN) {
                set_status("NAME TOO LONG");
                return;
            }
            file_name_input[len] = to_upper(ch);
            file_name_input[len + 1] = '\0';
        }
        return;
    }

    if (key == KEY_ESCAPE) {
        if (menu_open) {
            close_menu();
        } else {
            clear_document();
            set_status("DOCUMENT CLEARED");
        }
        return;
    }

    if (menu_open) {
        if (key == KEY_LEFT) {
            active_menu = (uint8_t)((active_menu + 3) % 4);
            active_item = 0;
        } else if (key == KEY_RIGHT) {
            active_menu = (uint8_t)((active_menu + 1) % 4);
            active_item = 0;
        } else if (key == KEY_UP) {
            const top_menu_t *menu = &MENUS[active_menu];
            active_item = (uint8_t)((active_item + menu->item_count - 1) % menu->item_count);
        } else if (key == KEY_DOWN) {
            const top_menu_t *menu = &MENUS[active_menu];
            active_item = (uint8_t)((active_item + 1) % menu->item_count);
        } else if (key == KEY_ENTER) {
            activate_current_item();
        }
        return;
    }

    if (key == KEY_LEFT) {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_line > 0) {
            cursor_line--;
            cursor_col = text_len(lines[cursor_line]);
        }
        return;
    }

    if (key == KEY_RIGHT) {
        uint8_t len = text_len(lines[cursor_line]);
        if (cursor_col < len) {
            cursor_col++;
        } else if (cursor_line + 1 < line_count) {
            cursor_line++;
            cursor_col = 0;
        }
        return;
    }

    if (key == KEY_UP) {
        if (cursor_line > 0) {
            cursor_line--;
            clamp_cursor();
        }
        return;
    }

    if (key == KEY_DOWN) {
        if (cursor_line + 1 < line_count) {
            cursor_line++;
            clamp_cursor();
        }
        return;
    }

    if (key == KEY_ENTER) {
        newline_at_cursor();
        return;
    }

    if (key == KEY_BACKSPACE) {
        backspace_at_cursor();
        return;
    }

    if (KEY_IS_CHAR(key)) {
        char ch = KEY_TO_CHAR(key);
        insert_char(ch);
    }
}

static uint8_t top_menu_at_x(int16_t x) {
    for (uint8_t i = 0; i < 4; ++i) {
        int mx = menu_x(i);
        if (x >= mx - 4 && x <= mx + 30) {
            return i;
        }
    }
    return 255;
}

static void editor_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button) {
    uint8_t left_edge = (uint8_t)(left_button && !prev_left_button);
    int16_t win_x = 16;
    int16_t win_y = 10;
    int16_t win_w = 288;
    int16_t win_h = 176;
    int doc_x;
    int doc_y;
    int doc_w;
    int doc_h;

    (void)exec_current_window_rect(&win_x, &win_y, &win_w, &win_h);
    doc_x = win_x + 8;
    doc_y = win_y + 40;
    doc_w = win_w - 16;
    doc_h = win_h - 64;

    if (about_open) {
        if (left_edge) {
            about_open = 0;
        }
        prev_left_button = left_button;
        return;
    }

    if (file_dialog_mode != FILE_DIALOG_NONE) {
        if (left_edge) {
            if (x >= 92 && x <= 144 && y >= 114 && y <= 126) {
                confirm_file_dialog();
            } else if (x >= 164 && x <= 224 && y >= 114 && y <= 126) {
                uint8_t was_save = (file_dialog_mode == FILE_DIALOG_SAVE) ? 1 : 0;
                close_file_dialog();
                set_status(was_save ? "SAVE CANCELED" : "OPEN CANCELED");
            }
        }
        prev_left_button = left_button;
        return;
    }

    if (right_button) {
        active_menu = 0;
        open_menu();
        prev_left_button = left_button;
        return;
    }

    if (!left_edge) {
        prev_left_button = left_button;
        return;
    }

    if (y >= win_y + 12 && y <= win_y + 26) {
        uint8_t idx = top_menu_at_x(x);
        if (idx != 255) {
            active_menu = idx;
            open_menu();
            prev_left_button = left_button;
            return;
        }
    }

    if (menu_open) {
        int dx = menu_x(active_menu) - 4;
        int dy = win_y + 26;
        int dw = 112;
        int dh = MENUS[active_menu].item_count * 12 + 2;
        if (x >= dx && x <= dx + dw && y >= dy && y <= dy + dh) {
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
        close_menu();
    } else if (x >= doc_x && x <= doc_x + doc_w && y >= doc_y && y <= doc_y + doc_h) {
        int rel_y = y - (doc_y + 6);
        int rel_x = x - (doc_x + 6);
        if (rel_y >= 0 && rel_x >= 0) {
            uint8_t line = (uint8_t)(rel_y / 9);
            uint8_t col = (uint8_t)(rel_x / 8);
            if (line >= line_count) {
                line = (uint8_t)(line_count - 1);
            }
            cursor_line = line;
            cursor_col = col;
            clamp_cursor();
            set_status("CURSOR MOVED");
        }
    }

    prev_left_button = left_button;
}

static const executable_header_t EDITOR_EXECUTABLE = {
    EEXE_MAGIC,
    EEXE_VERSION,
    "EDITOR",
    editor_start,
    editor_draw,
    editor_key,
    editor_mouse
};

const executable_header_t *text_editor_executable(void) {
    return &EDITOR_EXECUTABLE;
}
