#include "parse_net.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>

int build_net_json(const char *proc_net_dev, char *out, size_t outlen) {
  const char *p = proc_net_dev ? proc_net_dev : "";
  int first = 1, rc;
  jb b;

  if (!out || outlen == 0) return -1;
  jb_init(&b);
  jb_raw(&b, "[");
  while (*p) {
    const char *eol = strchr(p, '\n');
    const char *colon = memchr(p, ':', eol ? (size_t)(eol - p) : strlen(p));
    if (colon) {
      const char *n = p;
      char name[32], *end;
      size_t nl;
      long long f[16];
      int i;
      const char *q = colon + 1;
      while (n < colon && (*n == ' ' || *n == '\t')) n++;
      nl = (size_t)(colon - n);
      if (nl > 0 && nl < sizeof name && !(nl == 2 && n[0] == 'l' && n[1] == 'o')) {
        memcpy(name, n, nl);
        name[nl] = '\0';
        for (i = 0; i < 9; i++) {
          f[i] = strtoll(q, &end, 10);
          if (end == q) break;
          q = end;
        }
        if (i == 9) {
          if (!first) jb_raw(&b, ",");
          first = 0;
          jb_raw(&b, "{");
          jb_kstr(&b, "iface", name);
          jb_knum(&b, "rx_bytes", f[0]);
          jb_knum(&b, "tx_bytes", f[8]);
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
