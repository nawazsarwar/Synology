#include "parse_diskio.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_whole_disk(const char *n) {
  if (strlen(n) != 3) return 0;
  if (n[0] == 's' && n[1] == 'd' && n[2] >= 'a' && n[2] <= 'z') return 1;
  if (n[0] == 'm' && n[1] == 'd' && n[2] >= '0' && n[2] <= '9') return 1;
  return 0;
}

int build_diskio_json(const char *proc_diskstats, char *out, size_t outlen) {
  const char *p = proc_diskstats ? proc_diskstats : "";
  int first = 1, rc;
  jb b;

  if (!out || outlen == 0) return -1;
  jb_init(&b);
  jb_raw(&b, "[");
  while (*p) {
    const char *eol = strchr(p, '\n');
    char line[256], name[32];
    size_t ll = eol ? (size_t)(eol - p) : strlen(p);
    long long maj, min, f[8];
    int consumed = 0, i;
    if (ll < sizeof line) {
      char *q, *end;
      memcpy(line, p, ll);
      line[ll] = '\0';
      if (sscanf(line, "%lld %lld %31s%n", &maj, &min, name, &consumed) == 3 && is_whole_disk(name)) {
        q = line + consumed;
        /* f[0]=rd_ios f[1]=rd_merges f[2]=rd_sectors f[3]=rd_ms f[4]=wr_ios f[5]=wr_merges f[6]=wr_sectors */
        for (i = 0; i < 7; i++) {
          f[i] = strtoll(q, &end, 10);
          if (end == q) break;
          q = end;
        }
        if (i == 7) {
          if (!first) jb_raw(&b, ",");
          first = 0;
          jb_raw(&b, "{");
          jb_kstr(&b, "dev", name);
          jb_knum(&b, "rd_sectors", f[2]);
          jb_knum(&b, "wr_sectors", f[6]);
          jb_raw(&b, "}");
        }
      }
    }
    if (!eol) break;
    p = eol + 1;
  }
  jb_raw(&b, "]");

  if (!b.buf || b.len + 1 > outlen) rc = -1;
  else { memcpy(out, b.buf, b.len + 1); rc = (int)b.len; }
  jb_free(&b);
  return rc;
}
