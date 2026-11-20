/* Fixed-layout Lo-tech 86Box guest prototype.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ems_lotech.h"
#include <sys/stat.h>
#include <unistd.h>

#define LOCK_PATH "/tmp/ewems260.lock"
#define FRAME_ADDR 0xe0000000UL
#define PAGE_BYTES 16384U
#define PAGE_COUNT 8U
#define CAPACITY ((unsigned long)PAGE_COUNT * PAGE_BYTES)

typedef volatile unsigned char __far *frame_ptr;
static unsigned char active;
static pid_t owner;
static unsigned long extent;

static void map_page(unsigned char page)
{
    __asm__ volatile ("outb %0,%w1" : : "a"(page), "d"(0x260U) : "memory");
}

/* Require the tested emulator's Lo-tech register readback before touching
 * its page selectors or frame. A floating bus (FFh) is rejected read-only.
 * This is not an ISA device identification protocol or a physical probe. */
static int clean_emulated_registers(void)
{
    unsigned int port;
    for (port = 0x260; port <= 0x263; ++port) {
        unsigned char value;
        __asm__ volatile ("inb %w1,%0" : "=a"(value) : "d"(port) : "memory");
        if (value && (port != 0x260 || value != 0x80)) return 0;
    }
    return 1;
}

static int frame_clear_of_rom(void)
{
    unsigned int seg;
    for (seg = 0xc000; seg < 0xf000; seg += 0x80) {
        frame_ptr p = (frame_ptr)((unsigned long)seg << 16);
        if (p[0] == 0x55 && p[1] == 0xaa) {
            unsigned int size = p[2];
            if (seg >= 0xe000 || !size ||
                (unsigned long)seg + (unsigned long)size * 32U > 0xe000)
                return 0;
        }
    }
    return 1;
}

/* Save all probe bytes before writes (also safe if two banks alias). Restore
 * them on every exit, and use only this one explicitly reserved window. */
static int independent_pages(void)
{
    frame_ptr frame = (frame_ptr)FRAME_ADDR;
    unsigned char old[PAGE_COUNT];
    unsigned int page;
    int ok = 1;
    for (page = 0; page < PAGE_COUNT; page++) {
        map_page(page);
        old[page] = *frame;
    }
    for (page = 0; page < PAGE_COUNT; page++) {
        map_page(page);
        *frame = (unsigned char)(0x58 + page);
    }
    for (page = 0; page < PAGE_COUNT; page++) {
        map_page(page);
        if (*frame != 0x58 + page) ok = 0;
    }
    for (page = 0; page < PAGE_COUNT; page++) {
        map_page(page);
        *frame = old[page];
    }
    map_page(0);
    return ok;
}

int ew_lotech_open(void)
{
    /* Atomic cooperative lock between shim clients, not protection against
     * unrelated direct I/O. An unclean exit intentionally fails closed. */
    if (active || mkdir(LOCK_PATH, 0700) < 0) return -1;
    if (!clean_emulated_registers() || !frame_clear_of_rom() || !independent_pages()) {
        rmdir(LOCK_PATH);
        return -1;
    }
    owner = getpid();
    active = 1;
    return 0;
}

void ew_lotech_close(void)
{
    if (!active || owner != getpid()) return;
    extent = 0;
    map_page(0);
    active = 0;
    rmdir(LOCK_PATH);
}

int ew_lotech_alloc(unsigned long size)
{
    frame_ptr frame = (frame_ptr)FRAME_ADDR;
    unsigned int page = 0, piece, i;
    unsigned long left = size;
    if (!active || owner != getpid() || extent || !size || size > CAPACITY) return -1;
    /* Do not expose bytes written during an earlier application session. */
    while (left) {
        piece = left > PAGE_BYTES ? PAGE_BYTES : (unsigned int)left;
        map_page(page++);
        for (i = 0; i < piece; i++) frame[i] = 0;
        left -= piece;
    }
    map_page(0);
    extent = size;
    return 0;
}

void ew_lotech_free(void)
{
    if (active && owner == getpid()) extent = 0;
}

static int transfer(unsigned long off, unsigned char *dst,
                    const unsigned char *src, unsigned int len)
{
    frame_ptr frame = (frame_ptr)FRAME_ADDR;
    unsigned int page, in_page, piece, i;
    if (!active || owner != getpid() || !extent || off > extent || len > extent - off ||
        (len && !dst && !src)) return -1;
    page = (unsigned int)(off >> 14);
    in_page = (unsigned int)off & (PAGE_BYTES - 1);
    while (len) {
        piece = PAGE_BYTES - in_page;
        if (piece > len) piece = len;
        map_page(page++);
        if (src) {
            for (i = 0; i < piece; i++) frame[in_page + i] = *src++;
        } else {
            for (i = 0; i < piece; i++) *dst++ = frame[in_page + i];
        }
        in_page = 0;
        len -= piece;
    }
    map_page(0);
    return 0;
}

int ew_lotech_read(unsigned long off, void *dst, unsigned int len)
{
    return transfer(off, dst, 0, len);
}

int ew_lotech_write(unsigned long off, const void *src, unsigned int len)
{
    return transfer(off, 0, src, len);
}
