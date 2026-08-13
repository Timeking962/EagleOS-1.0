// EagleOS 1.0 Timer.
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/*
 * Initialize the Programmable Interval Timer.
 *
 * EagleOS uses the PIT as a polled time source for now.
 */
void timer_init(void);

/*
 * Update the timer.
 *
 * This should be called frequently from kernel_main().
 */
void timer_poll(void);

/*
 * Number of milliseconds elapsed since timer_init().
 */
uint32_t timer_get_milliseconds(void);

/*
 * Number of seconds elapsed since timer_init().
 */
uint32_t timer_get_seconds(void);

#endif /* TIMER_H */