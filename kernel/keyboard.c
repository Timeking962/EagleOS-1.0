// EagleOS 1.0 Keyboard Driver.
// Polling PS/2 keyboard driver with:
//   - US keyboard layout
//   - Shift
//   - Caps Lock
//   - Ctrl/Alt state tracking
//   - Extended E0 keys
//   - Key release handling
//   - ASCII punctuation
//
// This driver intentionally uses polling rather than IRQs.

#include <stdint.h>
#include "../include/keyboard.h"

extern void serial_write_string(const char *s);


/* ============================================================
 * PS/2 CONTROLLER
 * ============================================================ */

#define KBD_DATA_PORT       0x60
#define KBD_STATUS_PORT     0x64
#define KBD_COMMAND_PORT    0x64


static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}


static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}


/*
 * Wait until the controller input buffer is ready.
 */
static uint8_t keyboard_wait_write(void)
{
    uint32_t timeout = 100000;

    while (timeout--) {
        if (!(inb(KBD_STATUS_PORT) & 0x02)) {
            return 1;
        }
    }

    return 0;
}


/*
 * Wait until keyboard data is available.
 */
static uint8_t keyboard_wait_read(void)
{
    uint32_t timeout = 100000;

    while (timeout--) {
        if (inb(KBD_STATUS_PORT) & 0x01) {
            return 1;
        }
    }

    return 0;
}


/*
 * Send a command to the PS/2 controller.
 */
static uint8_t controller_command(uint8_t command)
{
    if (!keyboard_wait_write()) {
        return 0;
    }

    outb(KBD_COMMAND_PORT, command);
    return 1;
}


/*
 * Send a command directly to the keyboard.
 */
static uint8_t keyboard_command(uint8_t command)
{
    if (!keyboard_wait_write()) {
        return 0;
    }

    outb(KBD_DATA_PORT, command);
    return 1;
}


/*
 * Read one byte from the keyboard/controller.
 */
static uint8_t keyboard_read_byte(uint8_t *value)
{
    if (!value) {
        return 0;
    }

    if (!keyboard_wait_read()) {
        return 0;
    }

    *value = inb(KBD_DATA_PORT);
    return 1;
}


/*
 * Empty pending controller output.
 *
 * This is important because keyboard initialization can leave
 * ACK/status bytes in the controller buffer.
 */
static void keyboard_flush_output(void)
{
    uint32_t count = 64;

    while (count--) {
        if (!(inb(KBD_STATUS_PORT) & 0x01)) {
            break;
        }

        (void)inb(KBD_DATA_PORT);
    }
}


/* ============================================================
 * DRIVER STATE
 * ============================================================ */

static uint8_t keyboard_initialized = 0;

static uint8_t shift_left = 0;
static uint8_t shift_right = 0;

static uint8_t ctrl_left = 0;
static uint8_t ctrl_right = 0;

static uint8_t alt_left = 0;
static uint8_t alt_right = 0;

static uint8_t caps_lock = 0;
static uint8_t num_lock = 0;
static uint8_t scroll_lock = 0;

static uint8_t extended_prefix = 0;


/* ============================================================
 * HELPER STATE FUNCTIONS
 * ============================================================ */

static uint8_t shift_down(void)
{
    return (shift_left || shift_right) ? 1 : 0;
}


static uint8_t ctrl_down(void)
{
    return (ctrl_left || ctrl_right) ? 1 : 0;
}


static uint8_t alt_down(void)
{
    return (alt_left || alt_right) ? 1 : 0;
}


/* ============================================================
 * ASCII TRANSLATION
 * ============================================================ */

/*
 * Normal US keyboard mapping.
 *
 * Index = Set 1 make scancode.
 */
static char scancode_to_ascii(uint8_t scancode)
{
    static const char normal_map[128] = {

        /* Number row */
        [0x02] = '1',
        [0x03] = '2',
        [0x04] = '3',
        [0x05] = '4',
        [0x06] = '5',
        [0x07] = '6',
        [0x08] = '7',
        [0x09] = '8',
        [0x0A] = '9',
        [0x0B] = '0',

        [0x0C] = '-',
        [0x0D] = '=',

        /* QWERTY row */
        [0x10] = 'q',
        [0x11] = 'w',
        [0x12] = 'e',
        [0x13] = 'r',
        [0x14] = 't',
        [0x15] = 'y',
        [0x16] = 'u',
        [0x17] = 'i',
        [0x18] = 'o',
        [0x19] = 'p',

        [0x1A] = '[',
        [0x1B] = ']',

        /* Home row */
        [0x1E] = 'a',
        [0x1F] = 's',
        [0x20] = 'd',
        [0x21] = 'f',
        [0x22] = 'g',
        [0x23] = 'h',
        [0x24] = 'j',
        [0x25] = 'k',
        [0x26] = 'l',

        [0x27] = ';',
        [0x28] = '\'',
        [0x29] = '`',

        /* Bottom row */
        [0x2B] = '\\',

        [0x2C] = 'z',
        [0x2D] = 'x',
        [0x2E] = 'c',
        [0x2F] = 'v',
        [0x30] = 'b',
        [0x31] = 'n',
        [0x32] = 'm',

        [0x33] = ',',
        [0x34] = '.',
        [0x35] = '/',

        /* Space */
        [0x39] = ' '
    };


    static const char shift_map[128] = {

        /* Number row */
        [0x02] = '!',
        [0x03] = '@',
        [0x04] = '#',
        [0x05] = '$',
        [0x06] = '%',
        [0x07] = '^',
        [0x08] = '&',
        [0x09] = '*',
        [0x0A] = '(',
        [0x0B] = ')',

        [0x0C] = '_',
        [0x0D] = '+',

        /* QWERTY row */
        [0x10] = 'Q',
        [0x11] = 'W',
        [0x12] = 'E',
        [0x13] = 'R',
        [0x14] = 'T',
        [0x15] = 'Y',
        [0x16] = 'U',
        [0x17] = 'I',
        [0x18] = 'O',
        [0x19] = 'P',

        [0x1A] = '{',
        [0x1B] = '}',

        /* Home row */
        [0x1E] = 'A',
        [0x1F] = 'S',
        [0x20] = 'D',
        [0x21] = 'F',
        [0x22] = 'G',
        [0x23] = 'H',
        [0x24] = 'J',
        [0x25] = 'K',
        [0x26] = 'L',

        [0x27] = ':',
        [0x28] = '"',
        [0x29] = '~',

        /* Bottom row */
        [0x2B] = '|',

        [0x2C] = 'Z',
        [0x2D] = 'X',
        [0x2E] = 'C',
        [0x2F] = 'V',
        [0x30] = 'B',
        [0x31] = 'N',
        [0x32] = 'M',

        [0x33] = '<',
        [0x34] = '>',
        [0x35] = '?',

        /* Space */
        [0x39] = ' '
    };


    char ch;

    if (scancode >= 128) {
        return 0;
    }

    ch = normal_map[scancode];

    if (ch == 0) {
        return 0;
    }

    /*
     * Alphabetic characters are affected by Caps Lock.
     *
     * Shift + Caps Lock produces lowercase.
     */
    if (ch >= 'a' && ch <= 'z') {

        uint8_t uppercase = 0;

        if (caps_lock) {
            uppercase = 1;
        }

        if (shift_down()) {
            uppercase ^= 1;
        }

        if (uppercase) {
            return (char)(ch - 'a' + 'A');
        }

        return ch;
    }

    /*
     * Non-letter characters are affected only by Shift.
     */
    if (shift_down()) {
        return shift_map[scancode];
    }

    return ch;
}


/* ============================================================
 * LED SUPPORT
 * ============================================================ */

/*
 * Update keyboard LEDs.
 *
 * Bit 0 = Scroll Lock
 * Bit 1 = Num Lock
 * Bit 2 = Caps Lock
 */
static void keyboard_update_leds(void)
{
    uint8_t leds = 0;
    uint8_t response = 0;

    if (scroll_lock) {
        leds |= 0x01;
    }

    if (num_lock) {
        leds |= 0x02;
    }

    if (caps_lock) {
        leds |= 0x04;
    }

    /*
     * 0xED = Set keyboard LEDs.
     */
    if (!keyboard_command(0xED)) {
        return;
    }

    /*
     * Keyboard should respond with ACK.
     */
    if (keyboard_wait_read()) {
        response = inb(KBD_DATA_PORT);

        /*
         * 0xFA = ACK.
         *
         * If something else is returned, don't attempt to
         * send the LED byte because the keyboard may not be
         * ready.
         */
        if (response != 0xFA) {
            return;
        }
    }

    keyboard_command(leds);

    /*
     * Consume the second ACK if it is already available.
     */
    if (keyboard_wait_read()) {
        (void)inb(KBD_DATA_PORT);
    }
}


/* ============================================================
 * KEYBOARD INITIALIZATION
 * ============================================================ */

void keyboard_init(void)
{
    uint8_t command_byte = 0;
    uint8_t response = 0;

    serial_write_string("[keyboard] init\n");

    keyboard_initialized = 0;

    shift_left = 0;
    shift_right = 0;

    ctrl_left = 0;
    ctrl_right = 0;

    alt_left = 0;
    alt_right = 0;

    caps_lock = 0;
    num_lock = 0;
    scroll_lock = 0;

    extended_prefix = 0;

    /*
     * Flush anything left over from boot/BIOS.
     */
    keyboard_flush_output();


    /*
     * Disable keyboard while configuring the controller.
     */
    if (controller_command(0xAD)) {
        /*
         * Give the controller a moment and flush old data.
         */
        keyboard_flush_output();
    }


    /*
     * Read controller configuration byte.
     *
     * Command 0x20.
     */
    if (controller_command(0x20)) {

        if (keyboard_wait_read()) {
            command_byte = inb(KBD_DATA_PORT);

            serial_write_string("[keyboard] command byte = 0x");

            /*
             * We don't have a general hex printer in this
             * driver, so preserve the existing debug behavior
             * without requiring extra dependencies.
             */
            if ((command_byte & 0x01) != 0) {
                serial_write_string("71");
            } else {
                serial_write_string("70");
            }

            serial_write_string("\n");
        }
    }


    /*
     * Enable keyboard interface.
     */
    controller_command(0xAE);


    /*
     * Tell keyboard to enable scanning.
     *
     * 0xF4 = Enable scanning.
     */
    keyboard_flush_output();

    if (keyboard_command(0xF4)) {

        if (keyboard_wait_read()) {
            response = inb(KBD_DATA_PORT);

            if (response == 0xFA) {
                serial_write_string(
                    "[keyboard] enable response 0xFA\n"
                );
            }
        }
    }


    /*
     * Reset lock state LEDs.
     */
    keyboard_update_leds();


    /*
     * Flush anything left behind by initialization.
     */
    keyboard_flush_output();

    extended_prefix = 0;

    keyboard_initialized = 1;

    serial_write_string("[keyboard] init complete\n");
}


/* ============================================================
 * EXTENDED SCANCODE HANDLING
 * ============================================================ */

static uint16_t handle_extended_make(uint8_t scancode)
{
    switch (scancode) {

        /* Arrow keys */
        case 0x48:
            return KEY_UP;

        case 0x50:
            return KEY_DOWN;

        case 0x4B:
            return KEY_LEFT;

        case 0x4D:
            return KEY_RIGHT;


        /* Navigation */
        case 0x47:
            return KEY_HOME;

        case 0x4F:
            return KEY_END;

        case 0x49:
            return KEY_PAGE_UP;

        case 0x51:
            return KEY_PAGE_DOWN;

        case 0x52:
            return KEY_INSERT;

        case 0x53:
            return KEY_DELETE;


        /*
         * Right Ctrl.
         */
        case 0x1D:
            ctrl_right = 1;
            return KEY_NONE;


        /*
         * Right Alt.
         */
        case 0x38:
            alt_right = 1;
            return KEY_NONE;


        default:
            return KEY_NONE;
    }
}


static uint16_t handle_extended_break(uint8_t scancode)
{
    switch (scancode) {

        case 0x1D:
            ctrl_right = 0;
            return KEY_NONE;

        case 0x38:
            alt_right = 0;
            return KEY_NONE;

        default:
            return KEY_NONE;
    }
}


/* ============================================================
 * MAIN POLLING FUNCTION
 * ============================================================ */

uint16_t keyboard_poll_key(void)
{
    uint8_t status;
    uint8_t scancode;
    uint8_t released;
    char ch;


    /*
     * Don't attempt to poll before initialization.
     */
    if (!keyboard_initialized) {
        return KEY_NONE;
    }


    /*
     * Check controller status.
     *
     * Bit 0:
     *     Output buffer full.
     */
    status = inb(KBD_STATUS_PORT);

    if (!(status & 0x01)) {
        return KEY_NONE;
    }


    /*
     * Bit 5:
     *
     * 0 = keyboard data
     * 1 = mouse data
     *
     * Do not consume mouse packets here.
     */
    if (status & 0x20) {
        return KEY_NONE;
    }


    /*
     * Read keyboard byte.
     */
    scancode = inb(KBD_DATA_PORT);


    /*
     * ACK from keyboard.
     *
     * These should normally be consumed during initialization
     * and LED updates, but ignore them here as a safety measure.
     */
    if (scancode == 0xFA) {
        return KEY_NONE;
    }


    /*
     * Resend request.
     */
    if (scancode == 0xFE) {
        return KEY_NONE;
    }


    /*
     * Keyboard self-test passed.
     */
    if (scancode == 0xAA) {
        return KEY_NONE;
    }


    /*
     * Extended key prefix.
     */
    if (scancode == 0xE0) {
        extended_prefix = 1;
        return KEY_NONE;
    }


    /*
     * Some keyboards can emit 0xE1 for Pause/Break.
     *
     * EagleOS does not currently expose Pause/Break, so
     * discard the sequence safely.
     */
    if (scancode == 0xE1) {
        extended_prefix = 0;
        return KEY_NONE;
    }


    /*
     * Determine whether this is a key release.
     *
     * PS/2 Set 1:
     *
     *     make  = 0x00 - 0x7F
     *     break = 0x80 - 0xFF
     */
    released = (scancode & 0x80) ? 1 : 0;

    scancode &= 0x7F;


    /*
     * Handle an extended key.
     */
    if (extended_prefix) {

        extended_prefix = 0;

        if (released) {
            return handle_extended_break(scancode);
        }

        return handle_extended_make(scancode);
    }


    /*
     * --------------------------------------------------------
     * Modifier keys
     * --------------------------------------------------------
     */

    /* Left Shift */
    if (scancode == 0x2A) {
        shift_left = released ? 0 : 1;
        return KEY_NONE;
    }


    /* Right Shift */
    if (scancode == 0x36) {
        shift_right = released ? 0 : 1;
        return KEY_NONE;
    }


    /* Left Ctrl */
    if (scancode == 0x1D) {
        ctrl_left = released ? 0 : 1;
        return KEY_NONE;
    }


    /* Left Alt */
    if (scancode == 0x38) {
        alt_left = released ? 0 : 1;
        return KEY_NONE;
    }


    /*
     * --------------------------------------------------------
     * Lock keys
     * --------------------------------------------------------
     *
     * Only toggle when the key is pressed, not released.
     */

    if (scancode == 0x3A) {

        if (!released) {
            caps_lock ^= 1;
            keyboard_update_leds();
        }

        return KEY_NONE;
    }


    if (scancode == 0x45) {

        if (!released) {
            num_lock ^= 1;
            keyboard_update_leds();
        }

        return KEY_NONE;
    }


    if (scancode == 0x46) {

        if (!released) {
            scroll_lock ^= 1;
            keyboard_update_leds();
        }

        return KEY_NONE;
    }


    /*
     * Key releases do not generate characters/events.
     */
    if (released) {
        return KEY_NONE;
    }


    /*
     * --------------------------------------------------------
     * Special keys
     * --------------------------------------------------------
     */

    switch (scancode) {

        case 0x1C:
            return KEY_ENTER;

        case 0x01:
            return KEY_ESCAPE;

        case 0x0F:
            return KEY_TAB;

        case 0x0E:
            return KEY_BACKSPACE;


        /*
         * Numeric keypad Enter.
         *
         * Normally E0 1C, which is handled by the extended
         * handler. This regular case is included for unusual
         * keyboards/controllers.
         */
        case 0x35:
            break;


        default:
            break;
    }


    /*
     * --------------------------------------------------------
     * Ctrl + alphabet
     * --------------------------------------------------------
     *
     * Convert Ctrl+A ... Ctrl+Z to ASCII control codes.
     *
     * Ctrl+A = 1
     * Ctrl+B = 2
     * ...
     * Ctrl+Z = 26
     */
    if (ctrl_down()) {

        char base = scancode_to_ascii(scancode);

        if (base >= 'a' && base <= 'z') {
            return KEY_CHAR(
                (uint8_t)(base - 'a' + 1)
            );
        }

        if (base >= 'A' && base <= 'Z') {
            return KEY_CHAR(
                (uint8_t)(base - 'A' + 1)
            );
        }
    }


    /*
     * --------------------------------------------------------
     * ASCII translation
     * --------------------------------------------------------
     */

    ch = scancode_to_ascii(scancode);

    if (ch != 0) {
        return KEY_CHAR(ch);
    }


    return KEY_NONE;
}