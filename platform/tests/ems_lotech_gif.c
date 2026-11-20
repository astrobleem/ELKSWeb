/* Native single-image check for the same decoder/storage path ELKSWeb uses.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "../../image/gif.h"
#include "../../image/store.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    struct ew_gif conventional, stored;
    static unsigned char data[8192], row[160];
    FILE *file;
    unsigned int len, y, x;
    unsigned long at;
    int result = 5;
    if (argc != 2 || !(file = fopen(argv[1], "rb"))) return 1;
    len = fread(data, 1, sizeof(data), file);
    fclose(file);
    if (!len || len == sizeof(data)) return 2;
    if (ew_gif_decode_color(data, len, &conventional, 320, 200) != EW_GIF_OK)
        return 3;
    if (ew_gif_decode_color_store(data, len, &stored, 320, 200) != EW_GIF_OK) {
        ew_gif_free(&conventional);
        return 4;
    }
    if (stored.width != 320 || stored.height != 150 || stored.stride != sizeof(row) ||
        conventional.width != stored.width || conventional.height != stored.height ||
        conventional.stride != stored.stride || stored.masked || conventional.opacity ||
        !stored.store || stored.pixels || access("/tmp/ewems260.lock", F_OK)) goto done;
    for (y = 0; y < stored.height; y++) {
        at = (unsigned long)y * sizeof(row);
        if (ew_store_read(stored.store, at, row, sizeof(row))) goto done;
        for (x = 0; x < sizeof(row); ++x)
            if (row[x] != conventional.pixels[(unsigned int)at + x]) goto done;
    }
    result = 0;
done:
    ew_gif_free(&stored);
    ew_gif_free(&conventional);
    if (!result && access("/tmp/ewems260.lock", F_OK) == 0) return 6;
    if (!result) puts("LOTECH-GIF-320x150 EXACT-24000-BYTES RAM-PARITY-PASS");
    return result;
}
