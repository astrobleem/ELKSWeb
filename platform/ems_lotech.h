#ifndef ELKSWEB_EMS_LOTECH_H
#define ELKSWEB_EMS_LOTECH_H

/*
 * Experimental real-mode ELKS on 86Box only, direct Lo-tech session. The caller must
 * opt in on a dedicated 260h/E000h guest with this frame reserved, no UMB
 * covering it, no other EMS manager/client, and no fork of an active session.
 * This is not LIM/INT 67h or an interrupt hook. No raw I/O API is exported.
 */
int ew_lotech_open(void);
void ew_lotech_close(void);
int ew_lotech_alloc(unsigned long size);
void ew_lotech_free(void);
int ew_lotech_read(unsigned long off, void *dst, unsigned int len);
int ew_lotech_write(unsigned long off, const void *src, unsigned int len);

#endif
