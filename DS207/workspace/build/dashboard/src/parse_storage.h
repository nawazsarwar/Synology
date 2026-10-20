#ifndef DASH_PARSE_STORAGE_H
#define DASH_PARSE_STORAGE_H
#include <stddef.h>

/* Inputs are the CONTENTS of `df` (1K-blocks) and /proc/mdstat.
 * Emits {"volumes":[{mount,size_kb,used_kb,avail_kb}],"arrays":[{name,level,state}]}.
 * Volumes: mounts "/" and "/volume*". state: "ok"|"degraded"|"recovering".
 * Returns bytes written (excluding NUL) or -1 on overflow/NULL out. Bad input -> empty arrays. */
int build_storage_json(const char *df_out, const char *mdstat, char *out, size_t outlen);
#endif
