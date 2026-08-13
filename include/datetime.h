// EagleOS 1.0 Date and Time System.
#ifndef DATETIME_H
#define DATETIME_H

#include <stdint.h>
#include <stdbool.h>


/*
 * EagleOS system date/time structure.
 */
typedef struct {
    uint16_t year;

    uint8_t month;
    uint8_t day;

    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} eos_datetime_t;


/*
 * Initialize the date/time system.
 *
 * Reads the current date and time from the PC's CMOS RTC.
 */
void datetime_init(void);


/*
 * Get the current EagleOS date/time.
 */
void datetime_get(eos_datetime_t *datetime);


/*
 * Set the current software date/time.
 *
 * RTC hardware writing will be added in the next stage.
 */
void datetime_set(const eos_datetime_t *datetime);


/*
 * Advance the software clock by one second.
 */
void datetime_tick(void);


/*
 * Validate a date/time structure.
 */
bool datetime_is_valid(const eos_datetime_t *datetime);

#endif /* DATETIME_H */