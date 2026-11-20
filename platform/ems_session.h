#ifndef ELKSWEB_EMS_SESSION_H
#define ELKSWEB_EMS_SESSION_H

/* A private two-pipe, one-client session. No physical address is exported. */
#define EW_EMS_SESSION_FDS "ELKSWEB_EMS_FDS"
#define EW_EMS_REQUEST_SIZE 7U
#define EW_EMS_CHUNK 256U
#define EW_EMS_ALLOC 1
#define EW_EMS_READ 2
#define EW_EMS_WRITE 3
#define EW_EMS_FREE 4

int ew_ems_session_alloc(unsigned long bytes);
int ew_ems_session_copy(unsigned long offset, unsigned char *buf, unsigned count, int write);
void ew_ems_session_free(void);
int ew_ems_session_requested(void);
int ew_ems_io(int fd, unsigned char *buf, unsigned count, int writing);

#endif
