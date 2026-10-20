/* uiserver authentication: password hash verify + in-memory session tokens. C99/POSIX.
 *
 * Hash scheme (DECISION): vendored iterated SHA-256 + salt, NOT crypt().
 * Reason: host crypt() (macOS) only does DES, and musl/uClibc crypt support for
 * $5$/$6$ varies; the vendored scheme behaves identically on host, tests and PPC.
 * Stored format (one line, in ui/etc/passwd.hash):
 *     $s256$<salt>$<64 hex>
 *   salt  = 1..64 chars [A-Za-z0-9./]   (installer picks, e.g. 16 random hex)
 *   h0    = SHA256(salt || password)
 *   h(i)  = SHA256(h(i-1) || salt || password), i = 1..AUTH_ITERS   (raw 32-byte digests)
 *   hex   = lowercase hex of h(AUTH_ITERS)
 * Task 16 installer generates it with auth_make_hash() (small `dashpw` helper). */
#ifndef AUTH_H
#define AUTH_H
#include <stddef.h>
#include <time.h>

#define AUTH_ITERS 2000
#define AUTH_TOKEN_HEX 32            /* 128 bits from /dev/urandom */
#define AUTH_MAX_SESSIONS 32
#define AUTH_TTL_SECS (24 * 3600)
#define AUTH_PW_MAX 256

/* 1 if password matches hash in hash_file; 0 on mismatch/missing/unreadable/malformed. */
int auth_verify(const char *password, const char *hash_file);
/* Build "$s256$salt$hex" into out. 0 ok, -1 bad args/salt/overflow. */
int auth_make_hash(const char *password, const char *salt, char *out, size_t n);

/* Mint a session token (malloc'd, 32 hex chars; caller frees). NULL if no entropy. */
char *auth_new_token(void);
/* 1 if tok is a live (unexpired) session token. Constant-time over the table. */
int auth_check_token(const char *tok);
/* Current time as used for expiry/throttling (includes test offset). */
time_t auth_now(void);
/* Test hook: shift the clock used for expiry by secs. */
void auth_set_time_offset(long secs);
/* Reset all sessions (test hook / logout-all). */
void auth_clear_sessions(void);

/* Extract the sid value from a Cookie header ("a=b; sid=XXX; c=d"). 0 found, -1 not. */
int auth_cookie_sid(const char *cookie, char *out, size_t n);
/* 1 if the request's Cookie header carries a valid session. */
int auth_cookie_ok(const char *cookie);

#endif
