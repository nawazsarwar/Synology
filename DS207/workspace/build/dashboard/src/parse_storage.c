#include "parse_storage.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Copy next line of *pp into line (truncated to cap-1); advance *pp. Returns 0 at end. */
static int next_line(const char **pp, char *line, size_t cap) {
  const char *p = *pp, *eol;
  size_t ll;
  if (!*p) return 0;
  eol = strchr(p, '\n');
  ll = eol ? (size_t)(eol - p) : strlen(p);
  if (ll >= cap) ll = cap - 1;
  memcpy(line, p, ll);
  line[ll] = '\0';
  *pp = eol ? eol + 1 : p + strlen(p);
  return 1;
}

static int is_wanted_mount(const char *m) {
  return strcmp(m, "/") == 0 || strncmp(m, "/volume", 7) == 0;
}

static void add_volumes(jb *b, const char *df) {
  const char *p = df ? df : "";
  char line[512];
  int first = 1;
  while (next_line(&p, line, sizeof line)) {
    char fs[256], mount[256];
    long long size, used, avail;
    char pct[16];
    if (sscanf(line, "%255s %lld %lld %lld %15s %255s", fs, &size, &used, &avail, pct, mount) == 6 &&
        is_wanted_mount(mount)) {
      if (!first) jb_raw(b, ",");
      first = 0;
      jb_raw(b, "{");
      jb_kstr(b, "mount", mount);
      jb_knum(b, "size_kb", size);
      jb_knum(b, "used_kb", used);
      jb_knum(b, "avail_kb", avail);
      jb_raw(b, "}");
    }
  }
}

static void emit_array(jb *b, int *first, const char *name, const char *level, int degraded, int recovering) {
  if (!name[0]) return;
  if (!*first) jb_raw(b, ",");
  *first = 0;
  jb_raw(b, "{");
  jb_kstr(b, "name", name);
  jb_kstr(b, "level", level);
  jb_kstr(b, "state", recovering ? "recovering" : degraded ? "degraded" : "ok");
  jb_raw(b, "}");
}

static void add_arrays(jb *b, const char *md) {
  const char *p = md ? md : "";
  char line[512], name[32] = "", level[32] = "";
  int first = 1, degraded = 0, recovering = 0;
  while (next_line(&p, line, sizeof line)) {
    char n[32], st[32], lv[32];
    if (sscanf(line, "%31s : %31s %31s", n, st, lv) == 3 && n[0] == 'm' && n[1] == 'd' &&
        n[2] >= '0' && n[2] <= '9') {
      emit_array(b, &first, name, level, degraded, recovering);
      strcpy(name, n);
      strcpy(level, lv);
      degraded = recovering = 0;
    } else if (name[0]) {
      char *lb = strrchr(line, '[');
      if (strstr(line, "recovery") || strstr(line, "resync")) recovering = 1;
      else if (strstr(line, "blocks") && lb && (lb[1] == 'U' || lb[1] == '_')) {
        for (lb++; *lb && *lb != ']'; lb++)
          if (*lb == '_') degraded = 1;
      }
    }
  }
  emit_array(b, &first, name, level, degraded, recovering);
}

int build_storage_json(const char *df_out, const char *mdstat, char *out, size_t outlen) {
  jb b;
  int rc;
  if (!out || outlen == 0) return -1;
  jb_init(&b);
  jb_raw(&b, "{\"volumes\":[");
  add_volumes(&b, df_out);
  jb_raw(&b, "],\"arrays\":[");
  add_arrays(&b, mdstat);
  jb_raw(&b, "]}");
  if (!b.buf || b.len + 1 > outlen) rc = -1;
  else { memcpy(out, b.buf, b.len + 1); rc = (int)b.len; }
  jb_free(&b);
  return rc;
}
