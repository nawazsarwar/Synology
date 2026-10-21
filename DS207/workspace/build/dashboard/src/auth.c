#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

/* ---- vendored SHA-256 ---- */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t bl; } sha_ctx;
static const uint32_t K[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(sha_ctx *c, const unsigned char *p) {
  uint32_t w[64], a, b, cc, d, e, f, g, h, t1, t2; int i;
  for (i = 0; i < 16; i++) w[i] = (uint32_t)p[i*4] << 24 | (uint32_t)p[i*4+1] << 16 | (uint32_t)p[i*4+2] << 8 | p[i*4+3];
  for (i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
    uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  a=c->h[0]; b=c->h[1]; cc=c->h[2]; d=c->h[3]; e=c->h[4]; f=c->h[5]; g=c->h[6]; h=c->h[7];
  for (i = 0; i < 64; i++) {
    t1 = h + (ROR(e,6) ^ ROR(e,11) ^ ROR(e,25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
    t2 = (ROR(a,2) ^ ROR(a,13) ^ ROR(a,22)) + ((a & b) ^ (a & cc) ^ (b & cc));
    h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
  }
  c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d; c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}
static void sha_init(sha_ctx *c) {
  static const uint32_t i0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  memcpy(c->h, i0, sizeof i0); c->len = 0; c->bl = 0;
}
static void sha_update(sha_ctx *c, const void *d, size_t n) {
  const unsigned char *p = d;
  c->len += n;
  while (n) {
    size_t k = 64 - c->bl; if (k > n) k = n;
    memcpy(c->buf + c->bl, p, k); c->bl += k; p += k; n -= k;
    if (c->bl == 64) { sha_block(c, c->buf); c->bl = 0; }
  }
}
static void sha_final(sha_ctx *c, unsigned char out[32]) {
  uint64_t bits = c->len * 8; int i;
  unsigned char pad = 0x80, z = 0;
  sha_update(c, &pad, 1);
  while (c->bl != 56) sha_update(c, &z, 1);
  for (i = 7; i >= 0; i--) { unsigned char b = (unsigned char)(bits >> (i * 8)); sha_update(c, &b, 1); }
  for (i = 0; i < 8; i++) { out[i*4] = (unsigned char)(c->h[i] >> 24); out[i*4+1] = (unsigned char)(c->h[i] >> 16);
                            out[i*4+2] = (unsigned char)(c->h[i] >> 8); out[i*4+3] = (unsigned char)c->h[i]; }
}

/* constant-time equality over n bytes */
static int ct_eq(const void *a, const void *b, size_t n) {
  const volatile unsigned char *x = a, *y = b; unsigned char d = 0; size_t i;
  for (i = 0; i < n; i++) d |= (unsigned char)(x[i] ^ y[i]);
  return d == 0;
}

static int salt_ok(const char *s) {
  size_t i, l = s ? strlen(s) : 0;
  if (l == 0 || l > 64) return 0;
  for (i = 0; i < l; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '/')) return 0;
  }
  return 1;
}

static void derive_hex(const char *pw, const char *salt, char hex[65]) {
  unsigned char h[32]; sha_ctx c; int i;
  sha_init(&c); sha_update(&c, salt, strlen(salt)); sha_update(&c, pw, strlen(pw)); sha_final(&c, h);
  for (i = 0; i < AUTH_ITERS; i++) {
    sha_init(&c); sha_update(&c, h, 32); sha_update(&c, salt, strlen(salt)); sha_update(&c, pw, strlen(pw));
    sha_final(&c, h);
  }
  for (i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", h[i]);
}

int auth_make_hash(const char *pw, const char *salt, char *out, size_t n) {
  char hex[65];
  if (!pw || !out || !salt_ok(salt) || strlen(pw) > AUTH_PW_MAX) return -1;
  if (strlen(salt) + 6 + 64 + 1 > n) return -1;
  derive_hex(pw, salt, hex);
  sprintf(out, "$s256$%s$%s", salt, hex);
  return 0;
}

int auth_verify(const char *password, const char *hash_file) {
  char line[256], salt[65], hex[65], want[65];
  FILE *f; char *p, *q; size_t sl;
  if (!password || !hash_file || strlen(password) > AUTH_PW_MAX) return 0;
  f = fopen(hash_file, "r");
  if (!f) return 0;
  p = fgets(line, sizeof line, f);
  fclose(f);
  if (!p) return 0;
  line[strcspn(line, "\r\n")] = '\0';
  if (strncmp(line, "$s256$", 6) != 0) return 0;
  p = line + 6;
  q = strchr(p, '$');
  if (!q) return 0;
  sl = (size_t)(q - p);
  if (sl == 0 || sl > 64) return 0;
  memcpy(salt, p, sl); salt[sl] = '\0';
  if (!salt_ok(salt) || strlen(q + 1) != 64) return 0;
  memcpy(want, q + 1, 65);
  derive_hex(password, salt, hex);
  return ct_eq(hex, want, 64);
}

/* ---- sessions ---- */
static struct { char tok[AUTH_TOKEN_HEX + 1]; time_t exp; int used; } sess[AUTH_MAX_SESSIONS];
static long time_off = 0;
time_t auth_now(void) { return time(NULL) + (time_t)time_off; }
#define now auth_now
void auth_set_time_offset(long s) { time_off = s; }
void auth_clear_sessions(void) { memset(sess, 0, sizeof sess); }

char *auth_new_token(void) {
  unsigned char r[AUTH_TOKEN_HEX / 2]; size_t got = 0; int fd, i, slot = -1;
  char *t; time_t n = now();
  fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) return NULL;
  while (got < sizeof r) { ssize_t k = read(fd, r + got, sizeof r - got); if (k <= 0) break; got += (size_t)k; }
  close(fd);
  if (got != sizeof r) return NULL;
  t = malloc(AUTH_TOKEN_HEX + 1);
  if (!t) return NULL;
  for (i = 0; i < (int)sizeof r; i++) sprintf(t + i * 2, "%02x", r[i]);
  for (i = 0; i < AUTH_MAX_SESSIONS; i++)           /* free or expired slot first */
    if (!sess[i].used || sess[i].exp <= n) { slot = i; break; }
  if (slot < 0) {                                   /* full: evict the oldest */
    slot = 0;
    for (i = 1; i < AUTH_MAX_SESSIONS; i++) if (sess[i].exp < sess[slot].exp) slot = i;
  }
  memcpy(sess[slot].tok, t, AUTH_TOKEN_HEX + 1);
  sess[slot].exp = n + AUTH_TTL_SECS; sess[slot].used = 1;
  return t;
}

int auth_check_token(const char *tok) {
  int i, ok = 0; time_t n = now();
  if (!tok || strlen(tok) != AUTH_TOKEN_HEX) return 0;
  for (i = 0; i < AUTH_MAX_SESSIONS; i++)           /* no early exit */
    if (sess[i].used && sess[i].exp > n && ct_eq(sess[i].tok, tok, AUTH_TOKEN_HEX)) ok = 1;
  return ok;
}

void auth_drop_token(const char *tok) {
  int i;
  if (!tok || strlen(tok) != AUTH_TOKEN_HEX) return;
  for (i = 0; i < AUTH_MAX_SESSIONS; i++)
    if (sess[i].used && ct_eq(sess[i].tok, tok, AUTH_TOKEN_HEX)) memset(&sess[i], 0, sizeof sess[i]);
}

int auth_cookie_sid(const char *cookie, char *out, size_t n) {
  const char *p = cookie;
  if (!cookie || !out || n == 0) return -1;
  while (*p) {
    const char *e;
    size_t l;
    while (*p == ' ' || *p == ';' || *p == '\t') p++;
    e = p; while (*e && *e != ';') e++;
    if (strncmp(p, "sid=", 4) == 0) {
      l = (size_t)(e - (p + 4));
      if (l == 0 || l >= n) return -1;
      memcpy(out, p + 4, l); out[l] = '\0';
      return 0;
    }
    p = e;
  }
  return -1;
}

int auth_cookie_ok(const char *cookie) {
  char sid[AUTH_TOKEN_HEX + 8];
  if (auth_cookie_sid(cookie, sid, sizeof sid) < 0) return 0;
  return auth_check_token(sid);
}
