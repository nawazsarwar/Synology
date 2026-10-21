#ifndef DASH_PARSE_SMART_H
#define DASH_PARSE_SMART_H
#include <stddef.h>

/* smartctl_a = output of `smartctl -a /dev/<dev>` (v5.38). Emits
 * {dev, model, serial, health("ok"|"fail"|"unknown"), temp_c (-1 if no temp attribute)}.
 * Returns bytes written (excluding NUL) or -1 on overflow/NULL args. */
int build_smart_json(const char *dev, const char *smartctl_a, char *out, size_t outlen);
#endif
