/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Native check for the fixed-layout 86Box helper. */
#include "../platform/ems_session.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define REQUIRE(cond) do { if (!(cond)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(int argc, char **argv)
{
    unsigned char tx[380], rx[380];
    unsigned i;
    int absent = argc > 1 && !strcmp(argv[1], "absent");
    int die = argc > 1 && !strcmp(argv[1], "die");
    REQUIRE(ew_ems_session_requested());
    if (absent) {
        REQUIRE(ew_ems_session_alloc(22000UL) < 0);
        puts("PASS no Lo-tech board: conventional fallback sentinel");
        return 0;
    }
    REQUIRE(!ew_ems_session_alloc(22000UL));
    for (i = 0; i < sizeof(tx); ++i) tx[i] = (unsigned char)(i ^ (i >> 2) ^ 0x51);
    REQUIRE(!ew_ems_session_copy(16350UL, tx, sizeof(tx), 1));
    REQUIRE(!ew_ems_session_copy(16350UL, rx, sizeof(rx), 0));
    REQUIRE(!memcmp(tx, rx, sizeof(tx)));
    REQUIRE(ew_ems_session_copy(21900UL, tx, sizeof(tx), 1) < 0);
    REQUIRE(!ew_ems_session_copy(21900UL, rx, 100, 0));
    for (i = 0; i < 100; ++i) REQUIRE(rx[i] == 0);
    if (die) {
        puts("PASS boundary copy before forced client exit"); fflush(stdout);
        kill(getpid(), SIGKILL);
        return 99;
    }
    ew_ems_session_free();
    REQUIRE(!ew_ems_session_alloc(18000UL));
    REQUIRE(!ew_ems_session_copy(16350UL, rx, sizeof(rx), 0));
    for (i = 0; i < sizeof(rx); ++i) REQUIRE(rx[i] == 0);
    ew_ems_session_free();
    puts("PASS boundary copy, invalid range and zeroed reuse");
    return 0;
}
