#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int jb_reserve(jb *b, size_t extra) {
  size_t need = b->len + extra + 1, ncap;
  char *p;
  if (need <= b->cap) return 1;
  ncap = b->cap ? b->cap : 64;
  while (ncap < need) ncap *= 2;
  p = (char *)realloc(b->buf, ncap);
  if (!p) return 0;
  b->buf = p; b->cap = ncap;
  return 1;
}

void jb_init(jb *b) {
  b->buf = NULL; b->len = 0; b->cap = 0;
  if (jb_reserve(b, 0)) b->buf[0] = '\0';
}

void jb_free(jb *b) {
  free(b->buf);
  b->buf = NULL; b->len = b->cap = 0;
}

static void jb_append(jb *b, const char *s, size_t n) {
  if (!jb_reserve(b, n)) return;
  memcpy(b->buf + b->len, s, n);
  b->len += n;
  b->buf[b->len] = '\0';
}

void jb_raw(jb *b, const char *s) { if (s) jb_append(b, s, strlen(s)); }

static void jb_comma(jb *b) {
  if (b->len > 0) {
    char c = b->buf[b->len - 1];
    if (c != '{' && c != '[') jb_append(b, ",", 1);
  }
}

static void jb_escaped(jb *b, const char *s) {
  static const char hex[] = "0123456789abcdef";
  jb_append(b, "\"", 1);
  for (; s && *s; s++) {
    unsigned char c = (unsigned char)*s;
    switch (c) {
      case '"':  jb_append(b, "\\\"", 2); break;
      case '\\': jb_append(b, "\\\\", 2); break;
      case '\n': jb_append(b, "\\n", 2); break;
      case '\r': jb_append(b, "\\r", 2); break;
      case '\t': jb_append(b, "\\t", 2); break;
      case '\b': jb_append(b, "\\b", 2); break;
      case '\f': jb_append(b, "\\f", 2); break;
      default:
        if (c < 0x20) {
          char u[7] = { '\\', 'u', '0', '0', hex[c >> 4], hex[c & 15], 0 };
          jb_append(b, u, 6);
        } else {
          char ch = (char)c;
          jb_append(b, &ch, 1);
        }
    }
  }
  jb_append(b, "\"", 1);
}

static void jb_key(jb *b, const char *key) {
  jb_comma(b);
  jb_escaped(b, key);
  jb_append(b, ":", 1);
}

void jb_kstr(jb *b, const char *key, const char *val) {
  jb_key(b, key);
  jb_escaped(b, val ? val : "");
}

void jb_knum(jb *b, const char *key, long long val) {
  char t[32];
  jb_key(b, key);
  snprintf(t, sizeof t, "%lld", val);
  jb_raw(b, t);
}

void jb_kreal(jb *b, const char *key, double val) {
  char t[48];
  jb_key(b, key);
  if (val != val || val > 1e308 || val < -1e308) { jb_raw(b, "null"); return; } /* NaN/Inf invalid JSON */
  snprintf(t, sizeof t, "%.6g", val);
  jb_raw(b, t);
}
