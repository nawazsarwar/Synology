#ifndef DASH_PARSE_SESSIONS_H
#define DASH_PARSE_SESSIONS_H
#include <stddef.h>

/* Builds {"smb":[{"user","ip","share"}],"open_files":[{"user","path"}],
 * "ssh":[{"user","ip"}]} from `smbstatus` (Samba 4.x default layout: sessions,
 * tree-connect "Service" block, "Locked files" block) and `who` output.
 * Either input may be NULL/empty/malformed -> the matching array is []. share is ""
 * when no tree connect matches; ssh ip is "" when who shows no (host).
 * NOTE: smbstatus layout is the documented 4.x one; Task 12 verifies it on the
 * real binary. Returns bytes written (excl. NUL) or -1 on overflow/NULL out. */
int build_sessions_json(const char *smbstatus_out, const char *who_out,
                        char *out, size_t outlen);
#endif
