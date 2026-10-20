#ifndef DASH_PARSE_SERVICES_H
#define DASH_PARSE_SERVICES_H
#include <stddef.h>

/* One gathered service probe. version may be NULL (unknown/not running). */
typedef struct { const char *name; int up; const char *version; } svc_probe;

/* Formats probes as [{"name":..,"up":true|false,"version":"..."|null},...].
 * Pure formatting; probing lives in the collector. Returns bytes written
 * (excluding NUL) or -1 on overflow/NULL args/negative n. */
int build_services_json(const svc_probe *probes, int n, char *out, size_t outlen);
#endif
