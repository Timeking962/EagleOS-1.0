// EagleOS 1.0 system control implementation.
#include <stdint.h>
#include "../include/system.h"
#include "../include/version.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a" (value), "Nd" (port));
}

void system_reboot(void) {
    // Request CPU reset through keyboard controller command port.
    for (uint32_t i = 0; i < 100000u; ++i) {
        if ((inb(0x64) & 0x02u) == 0u) {
            outb(0x64, 0xFE);
        }
    }

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

const char *system_get_version(void) {
    return EAGLEOS_VERSION;
}

const char *system_get_build_tag(void) {
    return EAGLEOS_BUILD_TAG;
}

const char *GetVersion(void) {
    return system_get_version();
}

const char *GetBuildTag(void) {
    return system_get_build_tag();
}
