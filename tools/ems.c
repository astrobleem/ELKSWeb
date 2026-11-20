/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Supervise one stock-ELKS process using the opt-in 86Box Lo-tech backend. */
#include "../platform/ems_lotech.h"
#include "../platform/ems_session.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void serve(int from, int to, int active)
{
    unsigned char packet[EW_EMS_REQUEST_SIZE], data[EW_EMS_CHUNK], status, op;
    unsigned long arg;
    unsigned n;
    while (!ew_ems_io(from, packet, sizeof(packet), 0)) {
        op = packet[0];
        arg = packet[1] | (unsigned long)packet[2] << 8 |
            (unsigned long)packet[3] << 16 | (unsigned long)packet[4] << 24;
        n = packet[5] | (unsigned)packet[6] << 8;
        if (n > sizeof(data) || ((op != EW_EMS_READ && op != EW_EMS_WRITE) && n))
            break;
        if (op == EW_EMS_WRITE && ew_ems_io(from, data, n, 0)) break;
        status = 1;
        if (active) {
            switch (op) {
            case EW_EMS_ALLOC: status = ew_lotech_alloc(arg) != 0; break;
            case EW_EMS_READ: status = ew_lotech_read(arg, data, n) != 0; break;
            case EW_EMS_WRITE: status = ew_lotech_write(arg, data, n) != 0; break;
            case EW_EMS_FREE: ew_lotech_free(); status = 0; break;
            }
        }
        if (ew_ems_io(to, &status, 1, 1) ||
            (!status && op == EW_EMS_READ && ew_ems_io(to, data, n, 1))) break;
    }
}

int main(int argc, char **argv)
{
    int req[2], reply[2], active, status;
    pid_t child, result;
    char fds[40];
    /* Require a dedicated known 86Box guest; never auto-probe real hardware. */
    if (argc < 3 || strcmp(argv[1], "--86box-lotech")) {
        fprintf(stderr, "Usage: ems --86box-lotech program [args...]\n");
        return 2;
    }
    if (pipe(req) < 0) goto unavailable;
    if (pipe(reply) < 0) {
        close(req[0]); close(req[1]);
        goto unavailable;
    }
    child = fork();
    if (!child) {
        close(req[0]); close(reply[1]);
        sprintf(fds, EW_EMS_SESSION_FDS "=%u,%u", (unsigned)req[1], (unsigned)reply[0]);
        if (putenv(fds)) _exit(126);
        execvp(argv[2], &argv[2]);
        perror(argv[2]);
        _exit(127);
    }
    close(req[1]); close(reply[0]);
    if (child == (pid_t)-1) {
        close(req[0]); close(reply[1]);
        goto unavailable;
    }
    signal(SIGPIPE, SIG_IGN);
    active = ew_lotech_open() == 0;
    serve(req[0], reply[1], active);
    if (active) ew_lotech_close();
    close(req[0]); close(reply[1]);
    do { result = waitpid(child, &status, 0); } while (result == (pid_t)-1 && errno == EINTR);
    if (result != child) return 1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;

unavailable:
    /* No helper resources consumed; browser's ordinary RAM path remains. */
    execvp(argv[2], &argv[2]);
    perror(argv[2]);
    return 127;
}
