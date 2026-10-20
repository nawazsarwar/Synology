#include "http.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static int hexv(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int http_url_decode(const char *in, char *out, size_t n) {
  size_t o = 0;
  if (!in || !out || n == 0) return -1;
  while (*in) {
    unsigned char c = (unsigned char)*in;
    if (c == '%') {
      int h = hexv((unsigned char)in[1]), l = h < 0 ? -1 : hexv((unsigned char)in[2]);
      if (h < 0 || l < 0) return -1;
      c = (unsigned char)(h * 16 + l);
      if (c == 0) return -1;
      in += 3;
    } else in++;
    if (o + 1 >= n) return -1;
    out[o++] = (char)c;
  }
  out[o] = '\0';
  return 0;
}

int safe_path(const char *url_path, const char *root, char *resolved, size_t n) {
  char dec[HTTP_PATH_MAX];
  size_t rl, dl, i;
  const char *seg;
  if (!url_path || !root || !resolved || n == 0) return -1;
  resolved[0] = '\0';
  if (root[0] != '/') return -1;
  rl = strlen(root);
  while (rl > 1 && root[rl - 1] == '/') rl--;      /* normalise trailing slashes */
  if (rl == 1) return -1;                          /* refuse root == "/" */
  if (url_path[0] != '/') return -1;               /* must be origin-form absolute */
  if (url_path[1] == '/') return -1;               /* leading // */
  if (http_url_decode(url_path, dec, sizeof dec) < 0) return -1;
  dl = strlen(dec);
  for (i = 0; i < dl; i++) {
    unsigned char c = (unsigned char)dec[i];
    if (c < 0x20 || c == 0x7f || c == '\\') return -1;  /* ctrl chars, backslash */
  }
  /* segment check on decoded form: no "..", no ".", no empty segment */
  seg = dec + 1;
  while (dl > 1) {
    const char *e = strchr(seg, '/');
    size_t sl = e ? (size_t)(e - seg) : strlen(seg);
    if (sl == 0) { if (!e) break; return -1; }     /* "//" inside; trailing "/" ok */
    if (sl == 1 && seg[0] == '.') return -1;
    if (sl == 2 && seg[0] == '.' && seg[1] == '.') return -1;
    if (!e) break;
    seg = e + 1;
    if (*seg == '\0') break;
  }
  if (rl + (dl > 1 ? dl : 0) + 1 > n) return -1;
  memcpy(resolved, root, rl);
  if (dl > 1) memcpy(resolved + rl, dec, dl);
  resolved[rl + (dl > 1 ? dl : 0)] = '\0';
  /* final containment: root prefix AND a '/' (or end) boundary */
  if (strncmp(resolved, root, rl) != 0 ||
      (resolved[rl] != '\0' && resolved[rl] != '/')) { resolved[0] = '\0'; return -1; }
  return 0;
}

static int ieq(const char *a, const char *b, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
  return 1;
}

int http_parse_request(const char *raw, http_req *out) {
  const char *p, *sp1, *sp2, *eol, *q;
  size_t ml, tl, pl;
  char target[HTTP_PATH_MAX];
  if (!raw || !out) return -1;
  memset(out, 0, sizeof *out);
  out->content_length = -1;
  eol = strstr(raw, "\r\n");
  if (!eol) return -1;
  sp1 = memchr(raw, ' ', (size_t)(eol - raw));
  if (!sp1) return -1;
  sp2 = memchr(sp1 + 1, ' ', (size_t)(eol - sp1 - 1));
  if (!sp2) return -1;
  ml = (size_t)(sp1 - raw);
  tl = (size_t)(sp2 - sp1 - 1);
  if (ml == 0 || ml >= sizeof out->method || tl == 0 || tl >= sizeof target) return -1;
  if ((size_t)(eol - sp2 - 1) < 8 || strncmp(sp2 + 1, "HTTP/1.", 7) != 0) return -1;
  memcpy(out->method, raw, ml);
  memcpy(target, sp1 + 1, tl); target[tl] = '\0';
  if (target[0] != '/') return -1;
  q = strchr(target, '?');
  pl = q ? (size_t)(q - target) : tl;
  memcpy(out->path, target, pl); out->path[pl] = '\0';
  if (q) { const char *h; strncpy(out->query, q + 1, sizeof out->query - 1);
           if ((h = strchr(out->query, '#'))) out->query[h - out->query] = '\0'; }
  if (http_url_decode(out->path, out->dpath, sizeof out->dpath) < 0) out->dpath[0] = '\0';
  /* headers */
  p = eol + 2;
  while (*p && !(p[0] == '\r' && p[1] == '\n')) {
    const char *le = strstr(p, "\r\n");
    size_t ll;
    if (!le) break;
    ll = (size_t)(le - p);
    if (ll > 7 && ieq(p, "cookie:", 7)) {
      const char *v = p + 7; size_t vl;
      while (v < le && (*v == ' ' || *v == '\t')) v++;
      vl = (size_t)(le - v);
      if (vl >= sizeof out->cookie) return -1;
      memcpy(out->cookie, v, vl); out->cookie[vl] = '\0';
    } else if (ll > 15 && ieq(p, "content-length:", 15)) {
      char tmp[24]; const char *v = p + 15; size_t vl; char *end;
      while (v < le && *v == ' ') v++;
      vl = (size_t)(le - v);
      if (vl == 0 || vl >= sizeof tmp) return -1;
      memcpy(tmp, v, vl); tmp[vl] = '\0';
      out->content_length = strtol(tmp, &end, 10);
      if (*end || out->content_length < 0) return -1;
    }
    p = le + 2;
  }
  return 0;
}
