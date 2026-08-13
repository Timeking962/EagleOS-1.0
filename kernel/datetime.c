// EagleOS 1.0 CMOS RTC / Date and Time System.

#include <stdint.h>
#include <stdbool.h>

#include "../include/datetime.h"


/*
 * CMOS RTC I/O ports.
 *
 * Port 0x70:
 *     CMOS register selector
 *
 * Port 0x71:
 *     CMOS register data
 */
#define CMOS_INDEX_PORT 0x70
#define CMOS_DATA_PORT  0x71


/*
 * CMOS RTC registers.
 */
#define RTC_SECONDS     0x00
#define RTC_MINUTES     0x02
#define RTC_HOURS       0x04
#define RTC_DAY         0x07
#define RTC_MONTH       0x08
#define RTC_YEAR        0x09
#define RTC_STATUS_A    0x0A
#define RTC_STATUS_B    0x0B


/*
 * Current EagleOS software clock.
 */
static eos_datetime_t current_datetime;


/*
 * Basic x86 I/O functions.
 *
 * These use GCC inline assembly and therefore require an x86 build.
 */

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}


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
 * Read one CMOS register.
 */
static uint8_t cmos_read(uint8_t register_number) {
    outb(CMOS_INDEX_PORT, register_number);
    return inb(CMOS_DATA_PORT);
}


/*
 * Convert a Binary-Coded Decimal value to binary.
 */
static uint8_t bcd_to_binary(uint8_t value) {
    return (uint8_t)(
        ((value >> 4) * 10) +
        (value & 0x0F)
    );
}


/*
 * Check whether the RTC is currently updating.
 *
 * Bit 7 of Status Register A is set while the RTC is updating.
 */
static bool rtc_update_in_progress(void) {
    return (cmos_read(RTC_STATUS_A) & 0x80) != 0;
}


/*
 * Read the RTC twice until two consecutive readings match.
 *
 * This prevents us from accidentally reading:
 *
 * 08:59:59
 *
 * while the RTC changes to:
 *
 * 09:00:00
 *
 * between individual register reads.
 */
static void rtc_read_consistent(eos_datetime_t *datetime) {
    eos_datetime_t first;
    eos_datetime_t second;

    do {
        /*
         * Wait for an RTC update to finish.
         */
        while (rtc_update_in_progress()) {
        }

        /*
         * First reading.
         */
        first.second = cmos_read(RTC_SECONDS);
        first.minute = cmos_read(RTC_MINUTES);
        first.hour   = cmos_read(RTC_HOURS);

        first.day   = cmos_read(RTC_DAY);
        first.month = cmos_read(RTC_MONTH);
        first.year  = cmos_read(RTC_YEAR);

        /*
         * Second reading.
         */
        second.second = cmos_read(RTC_SECONDS);
        second.minute = cmos_read(RTC_MINUTES);
        second.hour   = cmos_read(RTC_HOURS);

        second.day   = cmos_read(RTC_DAY);
        second.month = cmos_read(RTC_MONTH);
        second.year  = cmos_read(RTC_YEAR);

    } while (
        first.second != second.second ||
        first.minute != second.minute ||
        first.hour != second.hour ||
        first.day != second.day ||
        first.month != second.month ||
        first.year != second.year
    );

    /*
     * Determine the RTC data format.
     *
     * Bit 2 of Status Register B:
     *
     * 0 = BCD
     * 1 = binary
     *
     * Bit 1:
     *
     * 0 = 12-hour
     * 1 = 24-hour
     */
    uint8_t status_b = cmos_read(RTC_STATUS_B);

    bool binary_mode = (status_b & 0x04) != 0;
    bool twenty_four_hour = (status_b & 0x02) != 0;

    /*
     * Convert BCD values if necessary.
     */
    if (!binary_mode) {
        second.second = bcd_to_binary(second.second);
        second.minute = bcd_to_binary(second.minute);
        second.hour   = bcd_to_binary(second.hour);
        second.day    = bcd_to_binary(second.day);
        second.month  = bcd_to_binary(second.month);
        second.year   = bcd_to_binary(second.year);
    }


    /*
     * Handle the RTC's 12-hour format.
     *
     * In 12-hour mode, bit 7 of the hour value is the PM flag.
     */
    if (!twenty_four_hour) {
        bool pm = (second.hour & 0x80) != 0;

        second.hour &= 0x7F;

        if (pm && second.hour < 12) {
            second.hour += 12;
        }

        if (!pm && second.hour == 12) {
            second.hour = 0;
        }
    }


    /*
     * RTC years are normally stored as 00-99.
     *
     * EagleOS currently assumes the RTC is in the 2000s.
     */
    second.year = (uint16_t)(2000 + second.year);

    *datetime = second;
}


/*
 * Validate a date/time structure.
 */
bool datetime_is_valid(const eos_datetime_t *datetime) {
    if (datetime == 0) {
        return false;
    }

    if (datetime->year < 1970 || datetime->year > 9999) {
        return false;
    }

    if (datetime->month < 1 || datetime->month > 12) {
        return false;
    }

    if (datetime->day < 1 || datetime->day > 31) {
        return false;
    }

    if (datetime->hour > 23) {
        return false;
    }

    if (datetime->minute > 59) {
        return false;
    }

    if (datetime->second > 59) {
        return false;
    }

    return true;
}


/*
 * Initialize EagleOS's software clock from the CMOS RTC.
 */
void datetime_init(void) {
    eos_datetime_t rtc_time;

    rtc_read_consistent(&rtc_time);

    if (datetime_is_valid(&rtc_time)) {
        current_datetime = rtc_time;
    } else {
        /*
         * Safe fallback if the RTC contains invalid data.
         */
        current_datetime.year = 2026;
        current_datetime.month = 1;
        current_datetime.day = 1;

        current_datetime.hour = 0;
        current_datetime.minute = 0;
        current_datetime.second = 0;
    }
}


/*
 * Return the current software date/time.
 */
void datetime_get(eos_datetime_t *datetime) {
    if (datetime == 0) {
        return;
    }

    *datetime = current_datetime;
}


/*
 * Set the software date/time.
 *
 * This currently does NOT write the CMOS RTC.
 *
 * We'll add that once the Settings application is implemented.
 */
void datetime_set(const eos_datetime_t *datetime) {
    if (!datetime_is_valid(datetime)) {
        return;
    }

    current_datetime = *datetime;
}


/*
 * Determine whether a year is a leap year.
 */
static bool is_leap_year(uint16_t year) {
    if ((year % 400) == 0) {
        return true;
    }

    if ((year % 100) == 0) {
        return false;
    }

    return (year % 4) == 0;
}


/*
 * Return the number of days in a month.
 */
static uint8_t days_in_month(uint16_t year, uint8_t month) {
    switch (month) {
        case 1:
            return 31;

        case 2:
            return is_leap_year(year) ? 29 : 28;

        case 3:
            return 31;

        case 4:
            return 30;

        case 5:
            return 31;

        case 6:
            return 30;

        case 7:
            return 31;

        case 8:
            return 31;

        case 9:
            return 30;

        case 10:
            return 31;

        case 11:
            return 30;

        case 12:
            return 31;

        default:
            return 0;
    }
}


/*
 * Advance the software clock by one second.
 */
void datetime_tick(void) {
    current_datetime.second++;

    if (current_datetime.second < 60) {
        return;
    }

    current_datetime.second = 0;
    current_datetime.minute++;

    if (current_datetime.minute < 60) {
        return;
    }

    current_datetime.minute = 0;
    current_datetime.hour++;

    if (current_datetime.hour < 24) {
        return;
    }

    current_datetime.hour = 0;
    current_datetime.day++;

    if (current_datetime.day <=
        days_in_month(current_datetime.year,
                      current_datetime.month)) {
        return;
    }

    current_datetime.day = 1;
    current_datetime.month++;

    if (current_datetime.month <= 12) {
        return;
    }

    current_datetime.month = 1;
    current_datetime.year++;
}