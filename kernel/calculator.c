// EagleOS 1.0 Calculator Program.
#include <stdint.h>
#include "../include/render_backend.h"
#include "../include/desktop_interface_manager.h"
#include "../include/keyboard.h"
#include "../include/exec.h"
#include "../include/system.h"

extern void serial_write_string(const char *s);

typedef enum calc_action {
    ACT_DIGIT_0,
    ACT_DIGIT_1,
    ACT_DIGIT_2,
    ACT_DIGIT_3,
    ACT_DIGIT_4,
    ACT_DIGIT_5,
    ACT_DIGIT_6,
    ACT_DIGIT_7,
    ACT_DIGIT_8,
    ACT_DIGIT_9,
    ACT_ADD,
    ACT_SUB,
    ACT_MUL,
    ACT_DIV,
    ACT_EQUALS,
    ACT_CLEAR_ALL,
    ACT_CLEAR_ENTRY,
    ACT_NEGATE,
    ACT_PERCENT,
    ACT_MENU
} calc_action_t;

typedef struct calc_button {
    const char *label;
    calc_action_t action;
} calc_button_t;

static const calc_button_t BUTTONS[5][4] = {
    { {"C", ACT_CLEAR_ALL}, {"CE", ACT_CLEAR_ENTRY}, {"+/-", ACT_NEGATE}, {"/", ACT_DIV} },
    { {"7", ACT_DIGIT_7},   {"8", ACT_DIGIT_8},      {"9", ACT_DIGIT_9},   {"*", ACT_MUL} },
    { {"4", ACT_DIGIT_4},   {"5", ACT_DIGIT_5},      {"6", ACT_DIGIT_6},   {"-", ACT_SUB} },
    { {"1", ACT_DIGIT_1},   {"2", ACT_DIGIT_2},      {"3", ACT_DIGIT_3},   {"+", ACT_ADD} },
    { {"MENU", ACT_MENU},   {"0", ACT_DIGIT_0},      {"%", ACT_PERCENT},   {"=", ACT_EQUALS} }
};

static int32_t accumulator = 0;
static int32_t display_value = 0;
static uint8_t pending_operation = 0; // 0 NONE, 1 ADD, 2 SUB, 3 MUL, 4 DIV
static uint8_t entering_number = 0;
static uint8_t error_state = 0;

static uint8_t selected_row = 3;
static uint8_t selected_col = 1;
static uint8_t prev_left_button = 0;
static uint8_t help_menu_open = 0;
static uint8_t about_open = 0;

#define CALC_WIN_X 52
#define CALC_WIN_Y 14
#define CALC_WIN_W 216
#define CALC_WIN_H 150
#define CALC_MENU_BAR_X 53
#define CALC_MENU_BAR_Y 26
#define CALC_MENU_BAR_W 214
#define CALC_HELP_X 222
#define CALC_HELP_Y 29
#define CALC_HELP_W 32
#define CALC_MENU_Y 41
#define CALC_DISPLAY_X 62
#define CALC_DISPLAY_Y 46
#define CALC_DISPLAY_W 196
#define CALC_DISPLAY_H 18
#define CALC_GRID_X 62
#define CALC_GRID_Y 72
#define CALC_BUTTON_W 44
#define CALC_BUTTON_H 14
#define CALC_BUTTON_GAP 4

static void serial_write_int(int32_t value) {
    char buf[12];
    int i = 0;
    uint8_t neg = 0;

    if (value < 0) {
        neg = 1;
        value = -value;
    }

    if (value == 0) {
        serial_write_string("0");
        return;
    }

    while (value > 0 && i < (int)sizeof(buf) - 1) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    if (neg && i < (int)sizeof(buf) - 1) {
        buf[i++] = '-';
    }

    while (i > 0) {
        char out[2];
        out[0] = buf[--i];
        out[1] = '\0';
        serial_write_string(out);
    }
}

static uint8_t text_len(const char *s) {
    uint8_t n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

static void draw_int(int x, int y, int32_t value, uint8_t color) {
    char buf[12];
    int i = 0;
    uint8_t neg = 0;

    if (value < 0) {
        neg = 1;
        value = -value;
    }

    if (value == 0) {
        graphics_text(x, y, "0", color);
        return;
    }

    while (value > 0 && i < (int)sizeof(buf) - 1) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    if (neg && i < (int)sizeof(buf) - 1) {
        buf[i++] = '-';
    }

    {
        int out = 0;
        char text[12];
        while (i > 0) {
            text[out++] = buf[--i];
        }
        text[out] = '\0';
        graphics_text(x, y, text, color);
    }
}

static const char *op_name(uint8_t op) {
    switch (op) {
        case 1: return "ADD";
        case 2: return "SUB";
        case 3: return "MUL";
        case 4: return "DIV";
        default: return "NONE";
    }
}

static void clear_all(void) {
    accumulator = 0;
    display_value = 0;
    pending_operation = 0;
    entering_number = 0;
    error_state = 0;
}

static void apply_pending(int32_t rhs) {
    if (pending_operation == 1) {
        accumulator += rhs;
    } else if (pending_operation == 2) {
        accumulator -= rhs;
    } else if (pending_operation == 3) {
        accumulator *= rhs;
    } else if (pending_operation == 4) {
        if (rhs == 0) {
            error_state = 1;
            return;
        }
        accumulator /= rhs;
    }
}

static void press_digit(uint8_t d) {
    if (error_state) {
        clear_all();
    }

    if (!entering_number) {
        display_value = (int32_t)d;
        entering_number = 1;
        return;
    }

    if (display_value >= 0) {
        if (display_value < 214748364) {
            display_value = display_value * 10 + (int32_t)d;
        }
    } else {
        if (display_value > -214748364) {
            display_value = display_value * 10 - (int32_t)d;
        }
    }
}

static void press_operator(uint8_t op) {
    if (error_state) {
        return;
    }

    if (pending_operation == 0) {
        accumulator = display_value;
    } else if (entering_number) {
        apply_pending(display_value);
        if (error_state) {
            return;
        }
        display_value = accumulator;
    }

    pending_operation = op;
    entering_number = 0;
}

static void press_equals(void) {
    if (error_state || pending_operation == 0) {
        return;
    }

    apply_pending(display_value);
    if (error_state) {
        return;
    }

    display_value = accumulator;
    pending_operation = 0;
    entering_number = 0;
}

static void perform_action(calc_action_t action) {
    if (action >= ACT_DIGIT_0 && action <= ACT_DIGIT_9) {
        press_digit((uint8_t)(action - ACT_DIGIT_0));
        return;
    }

    if (action == ACT_ADD) {
        press_operator(1);
    } else if (action == ACT_SUB) {
        press_operator(2);
    } else if (action == ACT_MUL) {
        press_operator(3);
    } else if (action == ACT_DIV) {
        press_operator(4);
    } else if (action == ACT_EQUALS) {
        press_equals();
        if (error_state) {
            serial_write_string("[calc] result=ERR\n");
        } else {
            serial_write_string("[calc] result=");
            serial_write_int(display_value);
            serial_write_string("\n");
        }
    } else if (action == ACT_CLEAR_ALL) {
        clear_all();
    } else if (action == ACT_CLEAR_ENTRY) {
        display_value = 0;
        entering_number = 0;
        error_state = 0;
    } else if (action == ACT_NEGATE) {
        if (!error_state) {
            display_value = -display_value;
            entering_number = 1;
        }
    } else if (action == ACT_PERCENT) {
        if (!error_state) {
            display_value /= 100;
            entering_number = 1;
        }
    } else if (action == ACT_MENU) {
        exec_launch("PROGMAN");
    }
}

static void draw_button(int x, int y, int w, int h, const char *label, uint8_t selected) {
    uint8_t fill = selected ? 8 : 7;
    graphics_rect(x, y, w, h, fill);
    graphics_box(x, y, w, h, 15);

    {
        uint8_t len = text_len(label);
        int tx = x + (w - ((int)len * 8)) / 2;
        int ty = y + (h - 8) / 2;
        if (tx < x + 2) {
            tx = x + 2;
        }
        graphics_text(tx, ty, label, selected ? 14 : 15);
    }
}

static uint8_t button_at_point(int16_t x, int16_t y, uint8_t *out_row, uint8_t *out_col) {
    for (uint8_t r = 0; r < 5; ++r) {
        for (uint8_t c = 0; c < 4; ++c) {
            int bx = CALC_GRID_X + c * (CALC_BUTTON_W + CALC_BUTTON_GAP);
            int by = CALC_GRID_Y + r * (CALC_BUTTON_H + CALC_BUTTON_GAP);
            if (x >= bx && x <= bx + CALC_BUTTON_W && y >= by && y <= by + CALC_BUTTON_H) {
                *out_row = r;
                *out_col = c;
                return 1;
            }
        }
    }
    return 0;
}

static void move_selection(int dr, int dc) {
    int nr = (int)selected_row + dr;
    int nc = (int)selected_col + dc;

    if (nr < 0) {
        nr = 0;
    }
    if (nr > 4) {
        nr = 4;
    }
    if (nc < 0) {
        nc = 0;
    }
    if (nc > 3) {
        nc = 3;
    }

    selected_row = (uint8_t)nr;
    selected_col = (uint8_t)nc;
}

static void calc_start(void) {
    clear_all();
    selected_row = 3;
    selected_col = 1;
    prev_left_button = 0;
    help_menu_open = 0;
    about_open = 0;
}

static void draw_menu_bar(void) {
    dim_draw_menu_title(CALC_HELP_X, CALC_HELP_Y, 38, "HELP", help_menu_open);
}

static void draw_help_menu(void) {
    if (!help_menu_open) {
        return;
    }
    dim_draw_menu_dropdown_frame(218, CALC_MENU_Y, 64, 14);
    dim_draw_menu_dropdown_item(218, CALC_MENU_Y + 2, 64, "ABOUT", 0);
}

static void draw_about_dialog(void) {
    if (!about_open) {
        return;
    }

    dim_draw_about_dialog(60, 54, 192, "ABOUT CALC", "CALC");
}

static void calc_draw(void) {
    dim_draw_app_window(CALC_WIN_X, CALC_WIN_Y, CALC_WIN_W, CALC_WIN_H, "CALCULATOR", CALC_MENU_BAR_X, CALC_MENU_BAR_Y, CALC_MENU_BAR_W, true);
    draw_menu_bar();

    graphics_rect(CALC_DISPLAY_X, CALC_DISPLAY_Y, CALC_DISPLAY_W, CALC_DISPLAY_H, 0);
    graphics_box(CALC_DISPLAY_X, CALC_DISPLAY_Y, CALC_DISPLAY_W, CALC_DISPLAY_H, 15);

    graphics_text(62, 62, "OP", 8);
    graphics_text(86, 62, op_name(pending_operation), 14);

    if (error_state) {
        graphics_text(CALC_DISPLAY_X + 8, CALC_DISPLAY_Y + 5, "ERR", 4);
    } else {
        draw_int(CALC_DISPLAY_X + 8, CALC_DISPLAY_Y + 5, display_value, 14);
    }

    for (uint8_t r = 0; r < 5; ++r) {
        for (uint8_t c = 0; c < 4; ++c) {
            int bx = CALC_GRID_X + c * (CALC_BUTTON_W + CALC_BUTTON_GAP);
            int by = CALC_GRID_Y + r * (CALC_BUTTON_H + CALC_BUTTON_GAP);
            uint8_t selected = (uint8_t)(r == selected_row && c == selected_col);
            draw_button(bx, by, CALC_BUTTON_W, CALC_BUTTON_H, BUTTONS[r][c].label, selected);
        }
    }

    draw_help_menu();
    draw_about_dialog();
}

static void calc_key(uint16_t key) {
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

    if (key == KEY_UP) {
        move_selection(-1, 0);
    } else if (key == KEY_DOWN) {
        move_selection(1, 0);
    } else if (key == KEY_LEFT) {
        move_selection(0, -1);
    } else if (key == KEY_RIGHT) {
        move_selection(0, 1);
    } else if (key == KEY_ENTER) {
        perform_action(BUTTONS[selected_row][selected_col].action);
    } else if (key == KEY_BACKSPACE) {
        perform_action(ACT_CLEAR_ENTRY);
    } else if (key == KEY_ESCAPE) {
        clear_all();
    } else if (KEY_IS_CHAR(key)) {
        char ch = KEY_TO_CHAR(key);
        if (ch >= '0' && ch <= '9') {
            perform_action((calc_action_t)(ACT_DIGIT_0 + (ch - '0')));
        } else if (ch == '+') {
            perform_action(ACT_ADD);
        } else if (ch == '-') {
            perform_action(ACT_SUB);
        } else if (ch == '*') {
            perform_action(ACT_MUL);
        } else if (ch == '/') {
            perform_action(ACT_DIV);
        } else if (ch == '=') {
            perform_action(ACT_EQUALS);
        } else if (ch == '%') {
            perform_action(ACT_PERCENT);
        } else if (ch == 'c' || ch == 'C') {
            perform_action(ACT_CLEAR_ALL);
        } else if (ch == 'h' || ch == 'H') {
            help_menu_open = 1;
        }
    }
}

static void calc_mouse(int16_t x, int16_t y, uint8_t left_button, uint8_t right_button) {
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
                if (x >= 218 && x <= 282 && y >= CALC_MENU_Y && y <= CALC_MENU_Y + 14) {
                    about_open = 1;
                }
                help_menu_open = 0;
                prev_left_button = left_button;
                return;
            }

            if (x >= CALC_HELP_X && x <= CALC_HELP_X + CALC_HELP_W && y >= CALC_HELP_Y && y <= CALC_HELP_Y + 10) {
                help_menu_open = 1;
                prev_left_button = left_button;
                return;
            }

            uint8_t row = 0;
            uint8_t col = 0;
            if (button_at_point(x, y, &row, &col)) {
                selected_row = row;
                selected_col = col;
                perform_action(BUTTONS[row][col].action);
            }
        }
    }

    prev_left_button = left_button;
}

static const executable_header_t CALC_EXECUTABLE = {
    EEXE_MAGIC,
    EEXE_VERSION,
    "CALC",
    calc_start,
    calc_draw,
    calc_key,
    calc_mouse
};

const executable_header_t *calculator_executable(void) {
    return &CALC_EXECUTABLE;
}
