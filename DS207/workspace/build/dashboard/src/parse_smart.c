#include "parse_smart.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Copy value after "key" at line start (skipping spaces) into dst, trimming trailing ws. */
static void line_value(const char *s, const char *key, char *dst, size_t n) {
  size_t kl = strlen(key), len;
  const char *p = s, *e;
  dst[0] = '\0';
  while (p && *p) {
    if (strncmp(p, key, kl) == 0) {
      p += kl;
      while (*p == ' ' || *p == '\t') p++;
      e = p;
      while (*e && *e != '\n' && *e != '\r') e++;
      while (e > p && (e[-1] == ' ' || e[-1] == '\t')) e--;
      len = (size_t)(e - p);
      if (len >= n) len = n - 1;
      memcpy(dst, p, len);
      dst[len] = '\0';
      return;
    }
    p = strchr(p, '\n');
    if (p) p++;
  }
}

static const char *health_of(const char *s) {
  char v[32];
  line_value(s, "SMART overall-health self-assessment test result:", v, sizeof v);
  if (strncmp(v, "PASSED", 6) == 0) return "ok";
  if (strncmp(v, "FAILED", 6) == 0) return "fail";
  return "unknown";
}

/* Attribute table row: "<id> <name> <flag> <value> <worst> <thresh> <type> <updated> <when_failed> <raw>".
 * Match ID 194 or name Temperature_Celsius; raw is the 10th token (leading integer). */
static long long temp_of(const char *s) {
  const char *p = s;
  while (p && *p) {
    const char *q = p;
    char *end;
    long id;
    int tok;
    while (*q == ' ') q++;
    id = strtol(q, &end, 10);
    if (end != q && (*end == ' ')) {
      const char *name = end;
      while (*name == ' ') name++;
      if (id == 194 || strncmp(name, "Temperature_Celsius", 19) == 0) {
        const char *t = q;
        for (tok = 0; tok < 9 && *t && *t != '\n'; tok++) {
          while (*t && *t != ' ' && *t != '\n') t++;
          while (*t == ' ') t++;
        }
        if (tok == 9 && isdigit((unsigned char)*t)) return strtoll(t, NULL, 10);
      }
    }
    p = strchr(p, '\n');
    if (p) p++;
  }
  return -1;
}

int build_smart_json(const char *dev, const char *smartctl_a, char *out, size_t outlen) {
  char model[128], serial[64];
  jb b;
  int rc;

  if (!out || outlen == 0) return -1;
  if (!dev) dev = "";
  if (!smartctl_a) smartctl_a = "";
  line_value(smartctl_a, "Device Model:", model, sizeof model);
  if (!model[0]) line_value(smartctl_a, "Model Family:", model, sizeof model);
  line_value(smartctl_a, "Serial Number:", serial, sizeof serial);

  jb_init(&b);
  jb_raw(&b, "{");
  jb_kstr(&b, "dev", dev);
  jb_kstr(&b, "model", model);
  jb_kstr(&b, "serial", serial);
  jb_kstr(&b, "health", health_of(smartctl_a));
  jb_knum(&b, "temp_c", temp_of(smartctl_a));
  jb_raw(&b, "}");

  if (!b.buf || b.len + 1 > outlen) rc = -1;
  else { memcpy(out, b.buf, b.len + 1); rc = (int)b.len; }
  jb_free(&b);
  return rc;
}
