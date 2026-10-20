#include "test.h"
#include "auth.h"
#include "uiserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char base[256];
static void wf(const char *rel, const char *txt) { char p[512]; FILE *f; snprintf(p, sizeof p, "%s/%s", base, rel);
  f = fopen(p, "w"); if (f) { fputs(txt, f); fclose(f); } }
static void setup(void) { char c[600], h[200], p[300];
  snprintf(base, sizeof base, "/tmp/auth_test_%d", (int)getpid());
  snprintf(c, sizeof c, "rm -rf %s; mkdir -p %s/www %s/state %s/etc", base, base, base, base); system(c);
  wf("www/index.html", "<h1>SECRETSHELL</h1>"); wf("state/system.json", "{\"ok\":1}");
  auth_make_hash("correct horse", "ab12CD.9/x", h, sizeof h);
  snprintf(p, sizeof p, "etc/passwd.hash"); wf(p, h); }

TEST(sha_known_vector) { /* guards the vendored hash against drift: installer must match */
  char h[200];
  ASSERT(auth_make_hash("pw", "salt", h, sizeof h) == 0);
  ASSERT(strncmp(h, "$s256$salt$", 11) == 0); ASSERT(strlen(h) == 11 + 64);
  ASSERT(auth_make_hash("pw", "bad salt!", h, sizeof h) == -1);
  ASSERT(auth_make_hash("pw", "", h, sizeof h) == -1);
  ASSERT(auth_make_hash("pw", "salt", h, 20) == -1);
}

TEST(verify_password) { char f[300];
  setup(); snprintf(f, sizeof f, "%s/etc/passwd.hash", base);
  ASSERT(auth_verify("correct horse", f) == 1);
  ASSERT(auth_verify("correct horsf", f) == 0);
  ASSERT(auth_verify("", f) == 0);
  ASSERT(auth_verify("correct horse", "/nonexistent/passwd.hash") == 0);
  ASSERT(auth_verify(NULL, f) == 0);
  wf("etc/bad.hash", "garbage\n"); snprintf(f, sizeof f, "%s/etc/bad.hash", base);
  ASSERT(auth_verify("correct horse", f) == 0);
  wf("etc/bad2.hash", "$s256$salt$abcd\n"); snprintf(f, sizeof f, "%s/etc/bad2.hash", base);
  ASSERT(auth_verify("x", f) == 0);
}

TEST(token_roundtrip) { char *a, *b; int i;
  a = auth_new_token(); b = auth_new_token();
  ASSERT(a && b); ASSERT(strlen(a) == 32); ASSERT(strcmp(a, b) != 0);
  ASSERT(auth_check_token(a) == 1); ASSERT(auth_check_token(b) == 1);
  ASSERT(auth_check_token("00000000000000000000000000000000") == 0);
  ASSERT(auth_check_token("") == 0); ASSERT(auth_check_token(NULL) == 0);
  ASSERT(auth_check_token("short") == 0);
  /* bounded: flooding evicts oldest but newest stays valid */
  for (i = 0; i < 100; i++) { char *t = auth_new_token(); ASSERT(t && auth_check_token(t)); if (i == 99) ASSERT(auth_check_token(t)); free(t); }
  free(a); free(b);
}

TEST(token_expiry) { char *a = auth_new_token();
  ASSERT(auth_check_token(a) == 1);
  auth_set_time_offset(AUTH_TTL_SECS - 10); ASSERT(auth_check_token(a) == 1);
  auth_set_time_offset(AUTH_TTL_SECS + 10); ASSERT(auth_check_token(a) == 0);
  auth_set_time_offset(0); free(a);
}

TEST(known_vector_and_dup_cookie) { char h[200]; http_req q;
  auth_make_hash("pw", "salt", h, sizeof h);
  ASSERT_STR_EQ(h, "$s256$salt$1b4d3509341786f7bd4977f976028ce46691f96362519da58e73a4a7fa9b0fb3");
  ASSERT(http_parse_request("GET / HTTP/1.0\r\nCookie: a=1\r\nCookie: sid=XYZ\r\n\r\n", &q) == 0);
  ASSERT_STR_EQ(q.cookie, "a=1; sid=XYZ");
  { char o[32]; ASSERT(auth_cookie_sid(q.cookie, o, sizeof o) == 0); ASSERT_STR_EQ(o, "XYZ"); }
}

TEST(cookie_parse) { char o[64];
  ASSERT(auth_cookie_sid("foo=bar; sid=XXX; baz=1", o, sizeof o) == 0); ASSERT_STR_EQ(o, "XXX");
  ASSERT(auth_cookie_sid("sid=ONLY", o, sizeof o) == 0); ASSERT_STR_EQ(o, "ONLY");
  ASSERT(auth_cookie_sid("xsid=no; mysid=no", o, sizeof o) == -1);
  ASSERT(auth_cookie_sid("", o, sizeof o) == -1);
  ASSERT(auth_cookie_sid("sid=", o, sizeof o) == -1);
  ASSERT(auth_cookie_sid("sid=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", o, 8) == -1);  /* too long */
}

/* run raw request through the gate; response text into buf */
static int run(const char *raw, const char *body, char *buf, size_t n) {
  http_req q; FILE *t = tmpfile(); size_t r; int st;
  if (http_parse_request(raw, &q) < 0) { fclose(t); return -1; }
  st = ui_handle(base, &q, body, body ? strlen(body) : 0, fileno(t));
  rewind(t); r = fread(buf, 1, n - 1, t); buf[r] = 0; fclose(t); return st;
}

TEST(gate_blocks_without_session) { char b[4096]; char raw[300];
  setup();
  ASSERT(run("GET /api/system HTTP/1.0\r\n\r\n", NULL, b, sizeof b) == 401);
  ASSERT(strncmp(b, "HTTP/1.0 401", 12) == 0); ASSERT(!strstr(b, "\"ok\""));
  ASSERT(run("GET /api/system HTTP/1.0\r\nCookie: sid=00000000000000000000000000000000\r\n\r\n", NULL, b, sizeof b) == 401);
  ASSERT(run("GET /api/system HTTP/1.0\r\nCookie: foo=bar\r\n\r\n", NULL, b, sizeof b) == 401);
  /* SPA shell: login page, never the real shell */
  ASSERT(run("GET / HTTP/1.0\r\n\r\n", NULL, b, sizeof b) == 200);
  ASSERT(strstr(b, "password")); ASSERT(!strstr(b, "SECRETSHELL"));
  ASSERT(run("GET /assets/app.js HTTP/1.0\r\n\r\n", NULL, b, sizeof b) == 302);
  ASSERT(strstr(b, "Location: /"));
  (void)raw;
}

TEST(login_flow) { char b[4096], raw[600], *sid, *e; int st;
  setup();
  st = run("POST /api/login HTTP/1.0\r\nContent-Length: 24\r\n\r\n", "{\"password\":\"wrong pass\"}", b, sizeof b);
  ASSERT(st == 401); ASSERT(!strstr(b, "Set-Cookie"));
  st = run("POST /api/login HTTP/1.0\r\n\r\n", "", b, sizeof b); ASSERT(st == 401);
  st = run("POST /api/login HTTP/1.0\r\n\r\n", "{\"password\":\"correct horse\"}", b, sizeof b);
  ASSERT(st == 200); ASSERT(strstr(b, "Set-Cookie: sid="));
  ASSERT(strstr(b, "HttpOnly")); ASSERT(strstr(b, "SameSite=Strict"));
  sid = strstr(b, "sid="); ASSERT(sid != NULL);
  if (sid) { char tok[64]; sid += 4; e = strchr(sid, ';'); ASSERT(e && e - sid == 32);
    memcpy(tok, sid, 32); tok[32] = 0;
    snprintf(raw, sizeof raw, "GET /api/system HTTP/1.0\r\nCookie: foo=bar; sid=%s; baz=1\r\n\r\n", tok);
    ASSERT(run(raw, NULL, b, sizeof b) == 200); ASSERT(strstr(b, "{\"ok\":1}"));
    snprintf(raw, sizeof raw, "GET / HTTP/1.0\r\nCookie: sid=%s\r\n\r\n", tok);
    ASSERT(run(raw, NULL, b, sizeof b) == 200); ASSERT(strstr(b, "SECRETSHELL"));
  }
  /* form-encoded, url-encoded value */
  st = run("POST /api/login HTTP/1.0\r\n\r\n", "password=correct%20horse", b, sizeof b); ASSERT(st == 200);
  st = run("POST /api/login HTTP/1.0\r\n\r\n", "x=1&password=correct+horse", b, sizeof b); ASSERT(st == 200);
  /* GET login is not a login */
  ASSERT(run("GET /api/login HTTP/1.0\r\n\r\n", NULL, b, sizeof b) == 401);
}

TEST(drop_token) { char *a, *b; auth_clear_sessions(); a = auth_new_token(); b = auth_new_token();
  ASSERT(auth_check_token(a)); ASSERT(auth_check_token(b));
  auth_drop_token(a);
  ASSERT(!auth_check_token(a)); ASSERT(auth_check_token(b));
  auth_drop_token(a); auth_drop_token(NULL); auth_drop_token("short");   /* no-ops */
  ASSERT(auth_check_token(b));
  free(a); free(b);
}

TEST(logout_flow) { char b[4096], raw[600]; char *tok = auth_new_token(); int st;
  setup();
  snprintf(raw, sizeof raw, "POST /api/logout HTTP/1.0\r\nCookie: sid=%s\r\n\r\n", tok);
  st = run(raw, NULL, b, sizeof b);
  ASSERT(st == 200); ASSERT(strstr(b, "Max-Age=0")); ASSERT(strstr(b, "Set-Cookie: sid=;"));
  ASSERT(!auth_check_token(tok));
  /* session gone: same cookie now 401 on logout and on data */
  ASSERT(run(raw, NULL, b, sizeof b) == 401);
  snprintf(raw, sizeof raw, "GET /api/system HTTP/1.0\r\nCookie: sid=%s\r\n\r\n", tok);
  ASSERT(run(raw, NULL, b, sizeof b) == 401);
  /* unauthenticated logout is refused */
  ASSERT(run("POST /api/logout HTTP/1.0\r\n\r\n", NULL, b, sizeof b) == 401);
  free(tok);
}

TEST(login_body_cap) { char b[4096]; char *big = malloc(20000); int st; http_req q; FILE *t;
  setup();
  memset(big, 'a', 19999); big[19999] = 0;
  /* declared Content-Length over cap rejected without reading the body */
  st = run("POST /api/login HTTP/1.0\r\nContent-Length: 638000\r\n\r\n", NULL, b, sizeof b);
  ASSERT(st == 413);
  /* actual body over cap rejected too */
  st = run("POST /api/login HTTP/1.0\r\nContent-Length: 10\r\n\r\n", big, b, sizeof b);
  ASSERT(st == 413);
  ASSERT(ui_body_ok(-1) == 1); ASSERT(ui_body_ok(0) == 1); ASSERT(ui_body_ok(UI_MAX_BODY) == 1);
  ASSERT(ui_body_ok(UI_MAX_BODY + 1) == 0);
  (void)q; (void)t;
  free(big);
}

TEST(login_throttle) { char b[4096]; int i, st = 0;
  setup(); auth_clear_sessions();
  for (i = 0; i < 12; i++) st = run("POST /api/login HTTP/1.0\r\n\r\n", "{\"password\":\"nope\"}", b, sizeof b);
  ASSERT(st == 429);
  /* even the right password is refused while locked out */
  ASSERT(run("POST /api/login HTTP/1.0\r\n\r\n", "{\"password\":\"correct horse\"}", b, sizeof b) == 429);
  auth_set_time_offset(120);
  ASSERT(run("POST /api/login HTTP/1.0\r\n\r\n", "{\"password\":\"correct horse\"}", b, sizeof b) == 200);
  auth_set_time_offset(0);
  { char c[300]; snprintf(c, sizeof c, "rm -rf %s", base); system(c); }
}
