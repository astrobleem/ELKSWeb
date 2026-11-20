/* App-scoped Lo-tech image store. No kernel EMS device or interrupt vector.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "elks_store.h"
#ifdef __ia16__
#include "ems_lotech.h"
#include "ems_session.h"
#include <stdlib.h>
#include <string.h>
#ifdef EW_STORE_TRACE
#include <stdio.h>
#include <unistd.h>
#endif
#endif

int ew_store_ems_requested(void)
{
#ifdef __ia16__
    const char *backend = getenv("ELKSWEB_EMS");
    if (backend && !strcmp(backend, "0")) return 0;
    return ew_ems_session_requested() || (backend && !strcmp(backend, "lotech"));
#else
    return 0;
#endif
}

unsigned ew_store_ems_alloc(unsigned long bytes)
{
#ifdef __ia16__
    if (!ew_store_ems_requested()) return 0;
    if (ew_ems_session_requested()) {
        if (ew_ems_session_alloc(bytes)) return 0;
#ifdef EW_STORE_TRACE
        fprintf(stderr, "ELKSWEB EMS helper allocate %lu bytes\n", bytes);
#endif
        return 2;
    }
    if (ew_lotech_open()) return 0;
    if (!ew_lotech_alloc(bytes)) {
#ifdef EW_STORE_TRACE
        fprintf(stderr, "ELKSWEB EMS Lo-tech PID=%u allocate %lu bytes\n",
                (unsigned)getpid(), bytes);
#endif
        return 1;
    }
    ew_lotech_close();
#else
    (void)bytes;
#endif
    return 0;
}

int ew_store_ems_copy(unsigned handle, unsigned long offset,
                      unsigned char *buffer, unsigned count, int write)
{
#ifdef __ia16__
    if (handle == 2) return ew_ems_session_copy(offset, buffer, count, write);
    if (handle != 1) return -1;
    return write ? ew_lotech_write(offset, buffer, count)
                 : ew_lotech_read(offset, buffer, count);
#else
    (void)handle; (void)offset; (void)buffer; (void)count; (void)write;
    return -1;
#endif
}

void ew_store_ems_free(unsigned handle)
{
#ifdef __ia16__
    if (handle == 2) {
        ew_ems_session_free();
#ifdef EW_STORE_TRACE
        fprintf(stderr, "ELKSWEB EMS helper released\n");
#endif
    } else if (handle == 1) {
        ew_lotech_free();
        ew_lotech_close();
#ifdef EW_STORE_TRACE
        fprintf(stderr, "ELKSWEB EMS Lo-tech PID=%u released\n", (unsigned)getpid());
#endif
    }
#else
    (void)handle;
#endif
}
