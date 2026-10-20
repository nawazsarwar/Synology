#ifndef UISERVER_H
#define UISERVER_H
#include "http.h"
#include <stddef.h>

/* Map a request to a file under base ("<base>/www" static, "<base>/state" api).
 * Returns HTTP status: 200 (file+ctype filled), 404, 405. Symlink-safe. */
int ui_resolve(const char *base, const http_req *q, char *file, size_t n, const char **ctype);
const char *ui_ctype(const char *path);
/* Write a complete HTTP/1.0 response (Connection: close) to fd. Returns status. */
int ui_serve(const char *base, const http_req *q, int fd);

#endif
