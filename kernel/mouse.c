// EagleOS 1.0 Mouse Code File.

#include <stdint.h>
#include "../include/mouse.h"


/* ------------------------------------------------------------------------- */
/* PS/2 controller ports                                                      */
/* ------------------------------------------------------------------------- */

#define PS2_DATA        0x60
#define PS2_STATUS      0x64
#define PS2_COMMAND     0x64


/* ------------------------------------------------------------------------- */
/* PS/2 status bits                                                           */
/* ------------------------------------------------------------------------- */

#define PS2_STATUS_OUTPUT_FULL       0x01
#define PS2_STATUS_INPUT_FULL        0x02
#define PS2_STATUS_AUX_DATA          0x20


/* ------------------------------------------------------------------------- */
/* PS/2 controller commands                                                   */
/* ------------------------------------------------------------------------- */

#define PS2_CMD_ENABLE_AUX           0xA8
#define PS2_CMD_READ_CONFIG          0x20
#define PS2_CMD_WRITE_CONFIG         0x60
#define PS2_CMD_WRITE_AUX            0xD4


/* ------------------------------------------------------------------------- */
/* Mouse commands                                                             */
/* ------------------------------------------------------------------------- */

#define MOUSE_CMD_SET_DEFAULTS      0xF6
#define MOUSE_CMD_ENABLE_DATA       0xF4


/* ------------------------------------------------------------------------- */
/* Mouse responses                                                            */
/* ------------------------------------------------------------------------- */

#define MOUSE_ACK                    0xFA


/* ------------------------------------------------------------------------- */
/* Low-level I/O                                                              */
/* ------------------------------------------------------------------------- */

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


/* ------------------------------------------------------------------------- */
/* Controller waits                                                            */
/* ------------------------------------------------------------------------- */

static uint8_t controller_wait_read(void)
{
    /*
     * Wait until the controller has data available.
     *
     * This is only used during initialization. Normal mouse polling
     * never waits for the controller.
     */
    for (uint32_t i = 0; i < 100000; ++i) {

        if (inb(PS2_STATUS) & PS2_STATUS_OUTPUT_FULL) {
            return 1;
        }
    }

    return 0;
}


static uint8_t controller_wait_write(void)
{
    /*
     * Wait until the controller input buffer is empty.
     */
    for (uint32_t i = 0; i < 100000; ++i) {

        if (!(inb(PS2_STATUS) & PS2_STATUS_INPUT_FULL)) {
            return 1;
        }
    }

    return 0;
}


/* ------------------------------------------------------------------------- */
/* Drain controller output                                                    */
/* ------------------------------------------------------------------------- */

static void controller_drain_output(void)
{
    /*
     * Remove stale keyboard/mouse responses before initialization.
     *
     * This prevents an old ACK or partial mouse packet from being
     * mistaken for the first byte of a new packet.
     */
    for (uint32_t i = 0; i < 64; ++i) {

        uint8_t status = inb(PS2_STATUS);

        if (!(status & PS2_STATUS_OUTPUT_FULL)) {
            break;
        }

        (void)inb(PS2_DATA);
    }
}


/* ------------------------------------------------------------------------- */
/* Send command to mouse                                                      */
/* ------------------------------------------------------------------------- */

static uint8_t mouse_write(uint8_t value)
{
    /*
     * Tell the PS/2 controller that the next byte belongs to
     * the auxiliary device.
     */
    if (!controller_wait_write()) {
        return 0;
    }

    outb(PS2_COMMAND, PS2_CMD_WRITE_AUX);

    if (!controller_wait_write()) {
        return 0;
    }

    outb(PS2_DATA, value);

    return 1;
}


/* ------------------------------------------------------------------------- */
/* Read a byte from the controller                                             */
/* ------------------------------------------------------------------------- */

static uint8_t controller_read_byte(uint8_t *value)
{
    if (!value) {
        return 0;
    }

    if (!controller_wait_read()) {
        return 0;
    }

    *value = inb(PS2_DATA);

    return 1;
}


/* ------------------------------------------------------------------------- */
/* Read mouse response                                                         */
/* ------------------------------------------------------------------------- */

static uint8_t mouse_read_response(uint8_t *value)
{
    /*
     * Mouse commands produce responses in the controller output buffer.
     *
     * We only accept a byte marked as AUX data.
     */
    for (uint32_t i = 0; i < 100000; ++i) {

        uint8_t status = inb(PS2_STATUS);

        if (!(status & PS2_STATUS_OUTPUT_FULL)) {
            continue;
        }

        if (!(status & PS2_STATUS_AUX_DATA)) {
            /*
             * Keyboard data is present.
             *
             * Consume it so it cannot block the controller while
             * waiting for the mouse response.
             */
            (void)inb(PS2_DATA);
            continue;
        }

        *value = inb(PS2_DATA);

        return 1;
    }

    return 0;
}


/* ------------------------------------------------------------------------- */
/* Send mouse command and wait for ACK                                        */
/* ------------------------------------------------------------------------- */

static uint8_t mouse_command(uint8_t command)
{
    uint8_t response = 0;

    if (!mouse_write(command)) {
        return 0;
    }

    if (!mouse_read_response(&response)) {
        return 0;
    }

    return response == MOUSE_ACK;
}


/* ------------------------------------------------------------------------- */
/* Mouse initialization                                                        */
/* ------------------------------------------------------------------------- */

void mouse_init(void)
{
    uint8_t config = 0;

    /*
     * Start with a clean controller output buffer.
     */
    controller_drain_output();


    /*
     * Enable the PS/2 auxiliary device.
     */
    if (controller_wait_write()) {
        outb(
            PS2_COMMAND,
            PS2_CMD_ENABLE_AUX
        );
    }


    /*
     * Read the controller configuration byte.
     */
    if (
        controller_wait_write()
    ) {
        outb(
            PS2_COMMAND,
            PS2_CMD_READ_CONFIG
        );
    }


    if (
        controller_read_byte(&config)
    ) {

        /*
         * Enable the auxiliary interrupt bit.
         *
         * EagleOS currently polls rather than using IRQ12,
         * but keeping the controller configuration correct is
         * still preferable.
         */
        config |= 0x02;

        /*
         * Disable keyboard IRQ bit if necessary? No.
         *
         * Preserve all other controller configuration bits.
         */

        if (controller_wait_write()) {

            outb(
                PS2_COMMAND,
                PS2_CMD_WRITE_CONFIG
            );

            if (controller_wait_write()) {
                outb(
                    PS2_DATA,
                    config
                );
            }
        }
    }


    /*
     * Clear any responses generated by controller setup.
     */
    controller_drain_output();


    /*
     * Reset mouse to its default settings.
     *
     * F6 should produce FA (ACK).
     */
    (void)mouse_command(
        MOUSE_CMD_SET_DEFAULTS
    );


    /*
     * Drain any remaining initialization data.
     */
    controller_drain_output();


    /*
     * Enable mouse data reporting.
     *
     * F4 should produce FA (ACK).
     */
    (void)mouse_command(
        MOUSE_CMD_ENABLE_DATA
    );


    /*
     * Clear anything left over before entering the normal
     * polling loop.
     */
    controller_drain_output();
}


/* ------------------------------------------------------------------------- */
/* Mouse polling                                                               */
/* ------------------------------------------------------------------------- */

uint8_t mouse_poll(
    int8_t *dx,
    int8_t *dy,
    uint8_t *left_button,
    uint8_t *right_button
)
{
    /*
     * Standard three-byte PS/2 mouse packet.
     */
    static uint8_t packet[3];

    /*
     * Number of bytes currently stored.
     */
    static uint8_t packet_index = 0;

    uint8_t status;
    uint8_t data;


    /*
     * Validate output pointers.
     */
    if (
        !dx ||
        !dy ||
        !left_button ||
        !right_button
    ) {
        return 0;
    }


    /*
     * Check controller status.
     *
     * IMPORTANT:
     *
     * This function NEVER waits for the mouse.
     *
     * If there is no data available, it immediately returns.
     */
    status = inb(PS2_STATUS);


    /*
     * No controller data available.
     */
    if (!(status & PS2_STATUS_OUTPUT_FULL)) {
        return 0;
    }


    /*
     * Data belongs to the keyboard, not the mouse.
     */
    if (!(status & PS2_STATUS_AUX_DATA)) {
        return 0;
    }


    /*
     * Read exactly one byte.
     */
    data = inb(PS2_DATA);


    /* --------------------------------------------------------------------- */
    /* First packet byte                                                      */
    /* --------------------------------------------------------------------- */

    if (packet_index == 0) {

        /*
         * The first byte of a standard PS/2 mouse packet always
         * has bit 3 set.
         */
        if (!(data & 0x08)) {

            /*
             * Invalid synchronization byte.
             *
             * Stay at byte zero and wait for the next AUX byte.
             */
            packet_index = 0;

            return 0;
        }

        packet[0] = data;
        packet_index = 1;

        return 0;
    }


    /* --------------------------------------------------------------------- */
    /* Second packet byte                                                     */
    /* --------------------------------------------------------------------- */

    if (packet_index == 1) {

        packet[1] = data;
        packet_index = 2;

        return 0;
    }


    /* --------------------------------------------------------------------- */
    /* Third packet byte                                                      */
    /* --------------------------------------------------------------------- */

    packet[2] = data;
    packet_index = 0;


    /* --------------------------------------------------------------------- */
    /* Validate packet                                                        */
    /* --------------------------------------------------------------------- */

    /*
     * First byte must still contain the synchronization bit.
     */
    if (!(packet[0] & 0x08)) {
        return 0;
    }


    /*
     * Ignore X/Y overflow packets.
     *
     * Bit 6 = X overflow
     * Bit 7 = Y overflow
     */
    if (packet[0] & 0xC0) {
        return 0;
    }


    /* --------------------------------------------------------------------- */
    /* Decode buttons                                                         */
    /* --------------------------------------------------------------------- */

    *left_button =
        (packet[0] & 0x01) ? 1 : 0;

    *right_button =
        (packet[0] & 0x02) ? 1 : 0;


    /* --------------------------------------------------------------------- */
    /* Decode movement                                                        */
    /* --------------------------------------------------------------------- */

    /*
     * Mouse byte 1 is X movement.
     * Mouse byte 2 is Y movement.
     *
     * The PS/2 mouse reports signed two's-complement movement,
     * so converting the bytes to int8_t is sufficient.
     */
    *dx = (int8_t)packet[1];

    *dy = (int8_t)packet[2];


    return 1;
}