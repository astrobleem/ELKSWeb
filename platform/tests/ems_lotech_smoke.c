#include "../ems_lotech.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    static const unsigned char want[] = {5, 9, 200, 1, 77, 128, 34, 89};
    unsigned char got[sizeof(want)];
    unsigned char fallback[sizeof(want)];
    const unsigned long edge = 16381UL;
    int no_board = argc == 2 && !strcmp(argv[1], "no-board");
    if (ew_lotech_open() < 0) {
        if (!no_board) return 1;
        memcpy(fallback, want, sizeof(want));
        memcpy(got, fallback, sizeof(got));
        if (memcmp(got, want, sizeof(got))) return 2;
        puts("LOTECH-NO-BOARD RAM-FALLBACK-PASS");
        return 0;
    }
    if (no_board || ew_lotech_alloc(32768UL) ||
        ew_lotech_write(edge, want, sizeof(want)) ||
        ew_lotech_read(edge, got, sizeof(got)) ||
        memcmp(got, want, sizeof(got)) ||
        !ew_lotech_write(32764UL, want, sizeof(want))) {
        ew_lotech_close();
        return 3;
    }
    ew_lotech_free();
    if (!ew_lotech_read(edge, got, sizeof(got)) || ew_lotech_alloc(32768UL) ||
        ew_lotech_read(edge, got, sizeof(got)) ||
        memcmp(got, "\0\0\0\0\0\0\0\0", sizeof(got))) {
        ew_lotech_close();
        return 4;
    }
    ew_lotech_close();
    puts("LOTECH-CROSS-PAGE FREE-REALLOC-ZERO PASS");
    return 0;
}
