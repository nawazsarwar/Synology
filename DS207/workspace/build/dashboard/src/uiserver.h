#ifndef UISERVER_H
#define UISERVER_H
#include "http.h"
#include <stddef.h>
#include <stdio.h>

/* Map a request to a file under base ("<base>/www" static, "<base>/state" api).
 * Returns HTTP status: 200 (file+ctype filled), 404, 405. Symlink-safe. */
int ui_resolve(const char *base, const http_req *q, char *file, size_t n, const char **ctype);
const char *ui_ctype(const char *path);
/* Write a complete HTTP/1.0 response (Connection: close) to fd. Returns status. */
int ui_serve(const char *base, const http_req *q, int fd);


/* Max accepted request body (login POST). Callers must reject larger before reading. */
#define UI_MAX_BODY 8192
/* 1 if a declared Content-Length (-1 = absent) is acceptable, 0 if over the cap. */
int ui_body_ok(long content_length);
/* Authenticated entry point (Task 11 main calls this): applies the session gate,
 * handles POST /api/login, else defers to ui_serve. body/blen = already-read request
 * body (may be NULL/0). Returns the HTTP status written to fd. */
int ui_handle(const char *base, const http_req *q, const char *body, size_t blen, int fd);

/* --make-hash: read one password line from `in`, take a random 16-char salt from the
 * entropy file rand_path (e.g. /dev/urandom), write "$s256$<salt>$<hex>\n" to `out`.
 * Returns 0 ok, nonzero on empty/oversize password or entropy failure. */
int ui_make_hash(FILE *in, FILE *out, const char *rand_path);

#endif
