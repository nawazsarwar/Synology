/* Minimal HTTP/1.x request parsing + traversal-safe path resolution. C99/POSIX. */
#ifndef HTTP_H
#define HTTP_H
#include <stddef.h>

#define HTTP_PATH_MAX 512
#define HTTP_COOKIE_MAX 512

typedef struct {
  char method[8];                 /* "GET", "POST", ... */
  char path[HTTP_PATH_MAX];       /* raw request path, query string stripped */
  char query[HTTP_PATH_MAX];      /* raw query string (no '?'), may be "" */
  char dpath[HTTP_PATH_MAX];      /* percent-decoded path; "" if invalid/contains NUL */
  char cookie[HTTP_COOKIE_MAX];   /* Cookie header value, "" if absent */
  long content_length;            /* -1 if absent */
} http_req;

/* Returns 0 on success, -1 on malformed/oversized request line. */
int http_parse_request(const char *raw, http_req *out);

/* Single-pass percent-decode. -1 on bad escape, decoded NUL, or overflow. */
int http_url_decode(const char *in, char *out, size_t n);

/* 0 and resolved (inside root, boundary-checked) or -1. Lexical; the caller
 * must still realpath() the final file to defeat symlinks. */
int safe_path(const char *url_path, const char *root, char *resolved, size_t n);

#endif
