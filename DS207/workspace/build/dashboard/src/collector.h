#ifndef DASH_COLLECTOR_H
#define DASH_COLLECTOR_H

/* Whitelisted tool ids. Each maps to a FIXED argv inside collector.c (no shell,
 * no request data). A provider returns malloc'd NUL-terminated output, or NULL. */
typedef enum {
  TOOL_DF, TOOL_SMART, TOOL_SMBSTATUS, TOOL_WHO, TOOL_NETSTAT, TOOL_PS,
  TOOL_VER_SMBD, TOOL_VER_SSH, TOOL_COUNT
} tool_id;

typedef char *(*tool_provider)(tool_id id);

/* Default provider: thin fork/exec wrapper over the whitelist. */
char *collector_exec_tool(tool_id id);
/* Swap provider (tests). NULL restores the exec wrapper. */
void collector_set_provider(tool_provider p);

/* One pass: writes system/net/diskio/storage/disks/services/sessions .json
 * atomically (tmp + rename) into statedir. /proc root = $COLLECTOR_PROC_DIR or /proc.
 * Returns 0 if all files written, else the count of failures. */
int collect_once(const char *statedir);
#endif
