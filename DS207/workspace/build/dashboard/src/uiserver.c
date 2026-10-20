/* uiserver serving logic (no auth: Task 10; no listen/main: Task 11). */
#include "uiserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define UI_MAX_FILE (4L * 1024 * 1024)

const char *ui_ctype(const char *path) {
  static const struct { const char *ext, *type; } t[] = {
    {"html", "text/html; charset=utf-8"}, {"js", "application/javascript"},
    {"mjs", "application/javascript"}, {"css", "text/css"},
    {"json", "application/json"}, {"svg", "image/svg+xml"}, {"png", "image/png"},
    {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"ico", "image/x-icon"},
    {"woff2", "font/woff2"}, {"woff", "font/woff"}, {"txt", "text/plain"},
    {"map", "application/json"}, {NULL, NULL}};
  const char *dot = strrchr(path, '.'), *slash = strrchr(path, '/');
  int i;
  if (dot && (!slash || dot > slash))
    for (i = 0; t[i].ext; i++) if (strcmp(dot + 1, t[i].ext) == 0) return t[i].type;
  return "application/octet-stream";
}

/* realpath(file) must lie under realpath(dir) with a '/' boundary. */
static int under_real(const char *dir, const char *file) {
  char rd[PATH_MAX], rf[PATH_MAX];
  size_t l;
  if (!realpath(dir, rd) || !realpath(file, rf)) return 0;
  l = strlen(rd);
  return strncmp(rf, rd, l) == 0 && rf[l] == '/';
}

static int api_name_ok(const char *s) {
  size_t i, l = strlen(s);
  if (l == 0 || l > 64) return 0;
  for (i = 0; i < l; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return 0;
  }
  return 1;
}

int ui_resolve(const char *base, const http_req *q, char *file, size_t n, const char **ctype) {
  char dir[PATH_MAX], tmp[PATH_MAX];
  struct stat st;
  if (!base || !q || !file || n == 0) return 404;
  if (strcmp(q->method, "GET") != 0) return 405;
  if (q->dpath[0] != '/') return 404;          /* decode failed / NUL */
  if (strncmp(q->dpath, "/api/", 5) == 0) {
    const char *name = q->dpath + 5;
    if (!api_name_ok(name)) return 404;
    if (snprintf(dir, sizeof dir, "%s/state", base) >= (int)sizeof dir) return 404;
    if (snprintf(tmp, sizeof tmp, "%s/%s.json", dir, name) >= (int)sizeof tmp) return 404;
  } else {
    if (snprintf(dir, sizeof dir, "%s/www", base) >= (int)sizeof dir) return 404;
    if (strcmp(q->dpath, "/") == 0) {
      if (snprintf(tmp, sizeof tmp, "%s/index.html", dir) >= (int)sizeof tmp) return 404;
    } else if (safe_path(q->path, dir, tmp, sizeof tmp) < 0) return 404;
  }
  if (stat(tmp, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size > UI_MAX_FILE) return 404;
  if (!under_real(dir, tmp)) return 404;       /* symlink escape */
  if (strlen(tmp) >= n) return 404;
  strcpy(file, tmp);
  if (ctype) *ctype = ui_ctype(tmp);
  return 200;
}

static int wall(int fd, const char *b, size_t l) {
  while (l) { ssize_t w = write(fd, b, l); if (w <= 0) return -1; b += w; l -= (size_t)w; }
  return 0;
}

int ui_serve(const char *base, const http_req *q, int fd) {
  char file[PATH_MAX], hdr[256];
  const char *ct = NULL;
  int st = ui_resolve(base, q, file, sizeof file, &ct), hl;
  char *body = NULL; size_t len = 0;
  if (st == 200) {
    int f = open(file, O_RDONLY);
    struct stat s;
    if (f < 0 || fstat(f, &s) < 0 || !S_ISREG(s.st_mode)) { if (f >= 0) close(f); st = 404; }
    else {
      body = malloc((size_t)s.st_size + 1);
      if (body) { ssize_t r; len = 0;
        while (len < (size_t)s.st_size && (r = read(f, body + len, (size_t)s.st_size - len)) > 0) len += (size_t)r; }
      else st = 404;
      close(f);
    }
  }
  if (st != 200) {
    const char *msg = st == 405 ? "Method Not Allowed" : "Not Found";
    hl = snprintf(hdr, sizeof hdr, "HTTP/1.0 %d %s\r\nContent-Type: text/plain\r\n"
                  "Content-Length: %d\r\nConnection: close\r\n\r\n", st, msg, (int)strlen(msg));
    wall(fd, hdr, (size_t)hl); wall(fd, msg, strlen(msg));
    return st;
  }
  hl = snprintf(hdr, sizeof hdr, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
                "X-Content-Type-Options: nosniff\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
                ct, (unsigned long)len);
  wall(fd, hdr, (size_t)hl); wall(fd, body, len);
  free(body);
  return 200;
}
