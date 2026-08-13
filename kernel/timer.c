// EagleOS 1.0 Programmable Interval Timer.

#include <stdint.h>

#include "../include/timer.h"
#include "../include/datetime.h"


#define PIT_CHANNEL0       0x40
#define PIT_COMMAND       0x43

#define PIT_BASE_FREQUENCY 1193182UL


/*
 * We program the PIT to approximately 1000 Hz.
 *
 * That gives us roughly one millisecond per PIT tick.
 */
#define PIT_FREQUENCY 1000UL

#define PIT_DIVISOR (PIT_BASE_FREQUENCY / PIT_FREQUENCY)


static uint32_t timer_milliseconds = 0;
static uint32_t timer_seconds = 0;

static uint16_t last_pit_count = 0;


/*
 * x86 port output.
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}


/*
 * x86 port input.
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}


/*
 * Read the current PIT counter.
 *
 * This uses PIT channel 0.
 */
static uint16_t pit_read_counter(void) {
    uint8_t low;
    uint8_t high;

    /*
     * Latch channel 0's current counter.
     */
    outb(PIT_COMMAND, 0x00);

    low = inb(PIT_CHANNEL0);
    high = inb(PIT_CHANNEL0);

    return (uint16_t)(
        ((uint16_t)high << 8) |
        low
    );
}


/*
 * Initialize PIT channel 0.
 */
void timer_init(void) {
    /*
     * Channel 0
     *
     * Access mode:
     *     low byte followed by high byte
     *
     * Mode 2:
     *     rate generator
     *
     * Binary mode.
     */
    outb(PIT_COMMAND, 0x34);

    /*
     * Load the divisor.
     */
    outb(PIT_CHANNEL0, (uint8_t)(PIT_DIVISOR & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF));

    timer_milliseconds = 0;
    timer_seconds = 0;

    last_pit_count = pit_read_counter();
}


/*
 * Update the software timer.
 */
void timer_poll(void) {
    uint16_t current_count;

    current_count = pit_read_counter();

    /*
     * The PIT counter counts downward.
     *
     * Normally:
     *
     *     last > current
     *
     * means that some fraction of the millisecond has passed.
     *
     * When the counter wraps around, current becomes larger
     * than last.
     */
    if (current_count <= last_pit_count) {
        /*
         * A normal downward movement occurred.
         */
        uint16_t elapsed_counts =
            (uint16_t)(last_pit_count - current_count);

        /*
         * Convert PIT counts into approximately milliseconds.
         */
        uint32_t elapsed_ms =
            ((uint32_t)elapsed_counts * 1000UL) /
            PIT_BASE_FREQUENCY;

        if (elapsed_ms > 0) {
            timer_milliseconds += elapsed_ms;

            while (timer_milliseconds >= 1000) {
                timer_milliseconds -= 1000;
                timer_seconds++;

                /*
                 * Advance EagleOS's software clock.
                 */
                datetime_tick();
            }
        }
    } else {
        /*
         * The PIT counter wrapped.
         *
         * Account for the remaining counts from the old
         * position to the reload value, plus the new count.
         */
        uint32_t elapsed_counts =
            (uint32_t)last_pit_count +
            (uint32_t)(PIT_DIVISOR - current_count);

        uint32_t elapsed_ms =
            (elapsed_counts * 1000UL) /
            PIT_BASE_FREQUENCY;

        if (elapsed_ms > 0) {
            timer_milliseconds += elapsed_ms;

            while (timer_milliseconds >= 1000) {
                timer_milliseconds -= 1000;
                timer_seconds++;

                datetime_tick();
            }
        }
    }

    last_pit_count = current_count;
}


/*
 * Return the number of milliseconds accumulated by the timer.
 */
uint32_t timer_get_milliseconds(void) {
    return timer_milliseconds;
}


/*
 * Return the number of seconds accumulated by the timer.
 */
uint32_t timer_get_seconds(void) {
    return timer_seconds;
}