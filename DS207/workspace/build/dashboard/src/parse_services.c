#include "parse_services.h"
#include "json.h"
#include <string.h>
#include <stdio.h>

int build_services_json(const svc_probe *probes, int n, char *out, size_t outlen) {
  int i, rc;
  jb b;

  if (!out || outlen == 0 || n < 0 || (n > 0 && !probes)) return -1;
  jb_init(&b);
  jb_raw(&b, "[");
  for (i = 0; i < n; i++) {
    if (i > 0) jb_raw(&b, ",");
    jb_raw(&b, "{");
    jb_kstr(&b, "name", probes[i].name);
    jb_raw(&b, probes[i].up ? ",\"up\":true" : ",\"up\":false");
    if (probes[i].version) {
      /* jb_kstr inserts the comma itself (previous char is not { or [) */
      jb_kstr(&b, "version", probes[i].version);
    } else {
      jb_raw(&b, ",\"version\":null");
    }
    if (probes[i].port > 0) {
      char pb[32];
      snprintf(pb, sizeof pb, ",\"port\":%d", probes[i].port);
      jb_raw(&b, pb);
    } else {
      jb_raw(&b, ",\"port\":null");
    }
    jb_raw(&b, "}");
  }
  jb_raw(&b, "]");

  if (!b.buf || b.len + 1 > outlen) rc = -1;
  else { memcpy(out, b.buf, b.len + 1); rc = (int)b.len; }
  jb_free(&b);
  return rc;
}
