// EagleOS 1.0 ATA PIO disk I/O.
#include <stdint.h>
#include "../include/disk.h"

#define ATA_IO_BASE 0x1F0
#define ATA_REG_DATA       (ATA_IO_BASE + 0)
#define ATA_REG_SECCOUNT0  (ATA_IO_BASE + 2)
#define ATA_REG_LBA0       (ATA_IO_BASE + 3)
#define ATA_REG_LBA1       (ATA_IO_BASE + 4)
#define ATA_REG_LBA2       (ATA_IO_BASE + 5)
#define ATA_REG_HDDEVSEL   (ATA_IO_BASE + 6)
#define ATA_REG_COMMAND    (ATA_IO_BASE + 7)
#define ATA_REG_STATUS     (ATA_IO_BASE + 7)

#define ATA_CMD_READ_PIO   0x20
#define ATA_CMD_WRITE_PIO  0x30
#define ATA_SR_BSY         0x80
#define ATA_SR_DRQ         0x08
#define ATA_SR_ERR         0x01
#define ATA_SR_DF          0x20

#define ATA_TIMEOUT 1000000u

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a" (value), "Nd" (port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a" (value) : "Nd" (port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a" (value), "Nd" (port));
}

static void io_wait_400ns(void) {
    (void)inb(ATA_REG_STATUS);
    (void)inb(ATA_REG_STATUS);
    (void)inb(ATA_REG_STATUS);
    (void)inb(ATA_REG_STATUS);
}

static int ata_wait_not_busy(void) {
    for (uint32_t i = 0; i < ATA_TIMEOUT; ++i) {
        uint8_t st = inb(ATA_REG_STATUS);
        if (st == 0xFF) {
            return 0;
        }
        if ((st & ATA_SR_BSY) == 0) {
            return 1;
        }
    }
    return 0;
}

static int ata_wait_drq_ready(void) {
    for (uint32_t i = 0; i < ATA_TIMEOUT; ++i) {
        uint8_t st = inb(ATA_REG_STATUS);
        if (st == 0xFF) {
            return 0;
        }
        if ((st & ATA_SR_ERR) || (st & ATA_SR_DF)) {
            return 0;
        }
        if ((st & ATA_SR_BSY) == 0 && (st & ATA_SR_DRQ)) {
            return 1;
        }
    }
    return 0;
}

static int ata_select_lba28(uint32_t lba) {
    if (lba > 0x0FFFFFFFu) {
        return 0;
    }

    if (!ata_wait_not_busy()) {
        return 0;
    }

    outb(ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait_400ns();

    outb(ATA_REG_SECCOUNT0, 1);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    return 1;
}

int disk_read_sector(uint32_t lba, uint8_t *buffer) {
    if (!buffer) {
        return 0;
    }

    if (!ata_select_lba28(lba)) {
        return 0;
    }

    outb(ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    if (!ata_wait_drq_ready()) {
        return 0;
    }

    for (uint16_t i = 0; i < 256; ++i) {
        uint16_t w = inw(ATA_REG_DATA);
        buffer[i * 2] = (uint8_t)(w & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(w >> 8);
    }

    io_wait_400ns();
    return 1;
}

int disk_write_sector(uint32_t lba, const uint8_t *buffer) {
    if (!buffer) {
        return 0;
    }

    if (!ata_select_lba28(lba)) {
        return 0;
    }

    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    if (!ata_wait_drq_ready()) {
        return 0;
    }

    for (uint16_t i = 0; i < 256; ++i) {
        uint16_t w = (uint16_t)buffer[i * 2] | ((uint16_t)buffer[i * 2 + 1] << 8);
        outw(ATA_REG_DATA, w);
    }

    io_wait_400ns();
    if (!ata_wait_not_busy()) {
        return 0;
    }

    return 1;
}
