/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ems_session.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* A short I/O is normal: stock ELKS has an 80-byte pipe buffer. */
int ew_ems_io(int fd, unsigned char *buf, unsigned count, int writing)
{
    int n;
    while (count) {
        n = writing ? write(fd,buf,count) : read(fd,buf,count);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        buf += n;
        count -= (unsigned)n;
    }
    return 0;
}

static int tx = -1, rx = -1;
static unsigned long extent;

static int parse_fd(const char **p)
{
    unsigned i = 0;
    if (**p < '0' || **p > '9') return -1;
    do {
        i = i * 10U + *(*p)++ - '0';
        if (i > 255U) return -1;
    } while (**p >= '0' && **p <= '9');
    return i < 3 ? -1 : (int)i;
}

static int pipe_fd(int fd, int direction)
{
    struct stat st;
    int mode = fcntl(fd, F_GETFL);
    return mode >= 0 && (mode & O_ACCMODE) == direction &&
        !fstat(fd, &st) && S_ISFIFO(st.st_mode);
}

int ew_ems_session_requested(void)
{
    const char *p = getenv(EW_EMS_SESSION_FDS);
    int out, in;
    if (tx >= 0) return 1;
    if (!p || (out = parse_fd(&p)) < 0 || *p++ != ',' ||
        (in = parse_fd(&p)) < 0 || *p || in == out ||
        !pipe_fd(out, O_WRONLY) || !pipe_fd(in, O_RDONLY))
        return 0;
    /* This process may not share the protocol connection across exec. */
    if (fcntl(out, F_SETFD, FD_CLOEXEC) < 0 ||
        fcntl(in, F_SETFD, FD_CLOEXEC) < 0)
        return 0;
    tx = out; rx = in;
    return 1;
}

static int request(unsigned char cmd, unsigned long arg, unsigned n,
                   unsigned char *buf)
{
    unsigned char packet[EW_EMS_REQUEST_SIZE], status;
    int failed;
    void (*handler)(int);
    if (!ew_ems_session_requested()) return -1;
    packet[0] = cmd;
    packet[1] = (unsigned char)arg;
    packet[2] = (unsigned char)(arg >> 8);
    packet[3] = (unsigned char)(arg >> 16);
    packet[4] = (unsigned char)(arg >> 24);
    packet[5] = (unsigned char)n;
    packet[6] = (unsigned char)(n >> 8);
    handler = signal(SIGPIPE, SIG_IGN);
    if (handler == SIG_ERR) return -1;
    failed = ew_ems_io(tx, packet, sizeof(packet), 1) ||
        (cmd == EW_EMS_WRITE && ew_ems_io(tx, buf, n, 1));
    signal(SIGPIPE, handler);
    if (failed || ew_ems_io(rx, &status, 1, 0)) return -1;
    if (status) return -1;
    return cmd == EW_EMS_READ ? ew_ems_io(rx, buf, n, 0) : 0;
}

int ew_ems_session_alloc(unsigned long bytes)
{
    if (!bytes || extent || request(EW_EMS_ALLOC, bytes, 0, 0)) return -1;
    extent = bytes;
    return 0;
}

int ew_ems_session_copy(unsigned long offset, unsigned char *buf, unsigned n,
                        int write)
{
    unsigned piece;
    if (!extent || offset > extent || n > extent - offset || (n && !buf)) return -1;
    /* Split for the helper's bounded near buffer; empty copies still validate. */
    do {
        piece = n > EW_EMS_CHUNK ? EW_EMS_CHUNK : n;
        if (request(write ? EW_EMS_WRITE : EW_EMS_READ, offset, piece, buf))
            return -1;
        if (!n) break;
        n -= piece; offset += piece; buf += piece;
    } while (n);
    return 0;
}

void ew_ems_session_free(void)
{
    if (extent && !request(EW_EMS_FREE, 0, 0, 0)) extent = 0;
}
