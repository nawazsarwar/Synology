#ifndef DASH_PARSE_SYSTEM_H
#define DASH_PARSE_SYSTEM_H
#include <stddef.h>

/* Inputs are the CONTENTS of /proc/stat, /proc/meminfo, /proc/loadavg, /proc/uptime.
 * Returns bytes written (excluding NUL) or -1 on overflow/NULL args. Missing fields are 0. */
int build_system_json(const char *stat, const char *meminfo, const char *loadavg,
                      const char *uptime, char *out, size_t outlen);
#endif
