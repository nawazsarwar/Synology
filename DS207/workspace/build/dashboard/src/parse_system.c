#include "parse_system.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>

/* Sum of all fields on the aggregate "cpu " line; *idle gets field 4. */
static void parse_cpu(const char *s, long long *total, long long *idle) {
  const char *p = s;
  int i;
  *total = *idle = 0;
  while (p && *p) {
    if (strncmp(p, "cpu ", 4) == 0) {
      char *end;
      p += 4;
      for (i = 1; ; i++) {
        long long v = strtoll(p, &end, 10);
        if (end == p) break;
        *total += v;
        if (i == 4) *idle = v;
        p = end;
      }
      return;
    }
    p = strchr(p, '\n');
    if (p) p++;
  }
}

static long long mem_kb(const char *s, const char *key) {
  size_t kl = strlen(key);
  const char *p = s;
  while (p && *p) {
    if (strncmp(p, key, kl) == 0 && p[kl] == ':') return strtoll(p + kl + 1, NULL, 10);
    p = strchr(p, '\n');
    if (p) p++;
  }
  return 0;
}

int build_system_json(const char *stat, const char *meminfo, const char *loadavg,
                      const char *uptime, char *out, size_t outlen) {
  long long total, idle;
  double l[3] = {0, 0, 0};
  const char *p;
  char *end;
  int i, rc;
  jb b;

  if (!out || outlen == 0) return -1;
  if (!stat) stat = "";
  if (!meminfo) meminfo = "";
  if (!loadavg) loadavg = "";
  if (!uptime) uptime = "";

  parse_cpu(stat, &total, &idle);
  p = loadavg;
  for (i = 0; i < 3; i++) {
    double v = strtod(p, &end);
    if (end == p) break;
    l[i] = v;
    p = end;
  }

  jb_init(&b);
  jb_raw(&b, "{");
  jb_knum(&b, "cpu_total", total);
  jb_knum(&b, "cpu_idle", idle);
  jb_knum(&b, "mem_total", mem_kb(meminfo, "MemTotal"));
  jb_knum(&b, "mem_free", mem_kb(meminfo, "MemFree"));
  jb_knum(&b, "mem_cached", mem_kb(meminfo, "Cached"));
  jb_kreal(&b, "load1", l[0]);
  jb_kreal(&b, "load5", l[1]);
  jb_kreal(&b, "load15", l[2]);
  jb_knum(&b, "uptime_s", (long long)strtod(uptime, NULL));
  jb_raw(&b, "}");

  if (!b.buf || b.len + 1 > outlen) rc = -1;
  else { memcpy(out, b.buf, b.len + 1); rc = (int)b.len; }
  jb_free(&b);
  return rc;
}
