#include "parse_wan.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LISTED 16   /* cap the public list; counts stay exact */

/* "C0A8010B" / "01BB" -> dotted quad / decimal port. Big-endian (see header). */
static int decode(const char *ah, const char *ph, unsigned char o[4], unsigned *port) {
  unsigned long a = strtoul(ah, NULL, 16);
  o[0] = (unsigned char)(a >> 24); o[1] = (unsigned char)(a >> 16);
  o[2] = (unsigned char)(a >> 8);  o[3] = (unsigned char)a;
  *port = (unsigned)strtoul(ph, NULL, 16);
  return 1;
}

static int is_lan(const unsigned char o[4]) {
  return o[0] == 10 || o[0] == 127 ||
         (o[0] == 172 && o[1] >= 16 && o[1] <= 31) ||
         (o[0] == 192 && o[1] == 168) ||
         (o[0] == 169 && o[1] == 254);
}

int build_wan_json(const char *proc_net_tcp, char *out, size_t outlen) {
  const char *p = proc_net_tcp ? proc_net_tcp : "";
  int npub = 0, nlan = 0, listed = 0, rc;
  jb b, list;

  if (!out || outlen == 0) return -1;
  jb_init(&b); jb_init(&list);
  while (*p) {
    const char *eol = strchr(p, '\n');
    char line[256], la[16], lp[16], ra[16], rp[16], st[8];
    size_t ll = eol ? (size_t)(eol - p) : strlen(p);
    if (ll >= sizeof line) ll = sizeof line - 1;
    memcpy(line, p, ll); line[ll] = '\0';
    if (sscanf(line, " %*d: %8[0-9A-Fa-f]:%4[0-9A-Fa-f] %8[0-9A-Fa-f]:%4[0-9A-Fa-f] %2[0-9A-Fa-f]",
               la, lp, ra, rp, st) == 5 &&
        strlen(la) == 8 && strlen(ra) == 8 && strcmp(st, "01") == 0) {   /* ESTABLISHED only */
      unsigned char lo[4], ro[4]; unsigned lport, rport;
      decode(la, lp, lo, &lport); decode(ra, rp, ro, &rport);
      if (is_lan(ro)) nlan++;
      else {
        npub++;
        if (listed < MAX_LISTED) {
          char l[32], r[32];
          snprintf(l, sizeof l, "%u.%u.%u.%u:%u", lo[0], lo[1], lo[2], lo[3], lport);
          snprintf(r, sizeof r, "%u.%u.%u.%u:%u", ro[0], ro[1], ro[2], ro[3], rport);
          if (listed) jb_raw(&list, ",");
          jb_raw(&list, "{"); jb_kstr(&list, "local", l);
          jb_kstr(&list, "remote", r); jb_raw(&list, "}");
          listed++;
        }
      }
    }
    if (!eol) break;
    p = eol + 1;
  }
  jb_raw(&b, "{\"public_conns\":[");
  if (list.buf) jb_raw(&b, list.buf);
  jb_raw(&b, "]");
  jb_knum(&b, "public_count", npub);
  jb_knum(&b, "lan_count", nlan);
  jb_raw(&b, "}");

  if (!b.buf || b.len + 1 > outlen) rc = -1;
  else { memcpy(out, b.buf, b.len + 1); rc = (int)b.len; }
  jb_free(&b); jb_free(&list);
  return rc;
}
