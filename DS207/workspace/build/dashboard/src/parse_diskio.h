#ifndef DASH_PARSE_DISKIO_H
#define DASH_PARSE_DISKIO_H
#include <stddef.h>

/* Input is the CONTENTS of /proc/diskstats. Emits JSON array of
 * {dev, rd_sectors, wr_sectors} for whole disks only (sd[a-z], md[0-9]).
 * Returns bytes written (excluding NUL) or -1 on overflow/NULL args. Bad input -> "[]". */
int build_diskio_json(const char *proc_diskstats, char *out, size_t outlen);
#endif
