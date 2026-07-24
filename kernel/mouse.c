// EagleOS 1.0 Mouse Code File.
#include <stdint.h>
#include "../include/mouse.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a" (value), "Nd" (port));
}

static void controller_drain_output(void) {
    for (uint32_t i = 0; i < 64; ++i) {
        if (!(inb(0x64) & 0x01)) {
            return;
        }
        (void)inb(0x60);
    }
}

static uint8_t controller_wait_read(void) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (inb(0x64) & 0x01) {
            return 1;
        }
    }
    return 0;
}

static uint8_t controller_wait_write(void) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (!(inb(0x64) & 0x02)) {
            return 1;
        }
    }
    return 0;
}

static void mouse_write(uint8_t value) {
    if (!controller_wait_write()) {
        return;
    }
    outb(0x64, 0xD4);
    if (!controller_wait_write()) {
        return;
    }
    outb(0x60, value);
}

static uint8_t mouse_read(uint8_t *value) {
    if (!controller_wait_read()) {
        return 0;
    }
    *value = inb(0x60);
    return 1;
}

void mouse_init(void) {
    uint8_t ignored = 0;

    controller_drain_output();

    if (!controller_wait_write()) {
        return;
    }
    outb(0x64, 0xA8);

    mouse_write(0xF6);
    mouse_read(&ignored);

    mouse_write(0xF4);
    mouse_read(&ignored);

    /* Drop any pending ACK/self-test bytes so packet parsing starts aligned. */
    controller_drain_output();
}

uint8_t mouse_poll(int8_t *dx, int8_t *dy, uint8_t *left_button, uint8_t *right_button) {
    static uint8_t packet[3];
    static uint8_t index = 0;

    uint8_t status = inb(0x64);
    if (!(status & 0x01) || !(status & 0x20)) {
        return 0;
    }

    uint8_t data = inb(0x60);

    if (index == 0) {
        /* Ignore non-packet responses that can appear around init/resync. */
        if (data == 0xFA || data == 0xAA || data == 0x00 || data == 0xFF) {
            return 0;
        }

        /* First byte must have the fixed sync bit set. */
        if (!(data & 0x08)) {
            return 0;
        }
    }

    packet[index++] = data;
    if (index < 3) {
        return 0;
    }

    index = 0;

    if (!(packet[0] & 0x08)) {
        return 0;
    }

    /* Drop overflow packets; they produce large bogus deltas. */
    if (packet[0] & 0xC0) {
        return 0;
    }

    *left_button = packet[0] & 0x01;
    *right_button = (packet[0] >> 1) & 0x01;
    *dx = (int8_t)packet[1];
    *dy = (int8_t)packet[2];
    return 1;
}
