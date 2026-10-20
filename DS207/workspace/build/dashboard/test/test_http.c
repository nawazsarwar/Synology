#include "test.h"
#include "http.h"
#include "uiserver.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#define R "/w/www"
static int sp(const char *u) { char r[512]; return safe_path(u, R, r, sizeof r); }

TEST(rejects_traversal) { char r[512];
  ASSERT(safe_path("/../../etc/shadow", "/w/www", r, sizeof r) == -1);
  ASSERT(safe_path("/..%2f..%2fetc/passwd", "/w/www", r, sizeof r) == -1);
  ASSERT(safe_path("/assets/app.js", "/w/www", r, sizeof r) == 0); }

TEST(traversal_vectors) {
  const char *bad[] = {
    "/..", "/../", "/a/..", "/a/../..", "/a/b/../../../x", "/assets/..",
    "/%2e%2e/etc/passwd", "/%2E%2E/etc/passwd", "/%2e%2E/x", "/%2e%2e%2fetc",
    "/%2e%2e%2f%2e%2e%2fetc/passwd", "/..%2fetc", "/..%2Fetc", "/..%5cetc", "/a%2f..%2f..%2fb",
    "/.%2e/x", "/%2e./x", "/a/%2e%2e", "/a/%2e%2e/",
    "/a%00.js", "/a%00", "/%00", "/assets/app.js%00.png",
    "//etc/passwd", "//", "///x", "/a//b",
    "etc/passwd", "", "..", "../x", "\\..\\x", "/a\\b", "/a/..\\b",
    "/%zz", "/%2", "/%", "/a%g1",
    "/./x", "/a/./b", "/%2e/x",
    "/a\nb", "/a\rb", "/a\x01", "/a\x7f",
    NULL };
  int i;
  for (i = 0; bad[i]; i++) if (sp(bad[i]) != -1) { printf("    accepted bad: [%s]\n", bad[i]); test_failed++; }
}

TEST(accepts_good_and_stays_inside) {
  char r[512];
  ASSERT(safe_path("/assets/app.js", R, r, sizeof r) == 0); ASSERT_STR_EQ(r, "/w/www/assets/app.js");
  ASSERT(safe_path("/", R, r, sizeof r) == 0); ASSERT_STR_EQ(r, "/w/www");
  ASSERT(safe_path("/a%20b.css", R, r, sizeof r) == 0); ASSERT_STR_EQ(r, "/w/www/a b.css");
  ASSERT(safe_path("/a..b/c...d", R, r, sizeof r) == 0);       /* dots inside names are fine */
  ASSERT(safe_path("/%252e%252e/x", R, r, sizeof r) == 0);     /* single decode: literal %2e%2e dir */
  ASSERT_STR_EQ(r, "/w/www/%2e%2e/x");
  ASSERT(safe_path("/x", "/w/www/", r, sizeof r) == 0); ASSERT_STR_EQ(r, "/w/www/x"); /* trailing / root */
}

TEST(prefix_collision_and_bad_roots) {
  char r[512];
  /* sibling "/w/wwwx" must never be reachable, however it is spelled */
  ASSERT(sp("/../wwwx/secret") == -1);
  ASSERT(sp("/..%2fwwwx/secret") == -1);
  ASSERT(safe_path("/x", "relative/root", r, sizeof r) == -1);
  ASSERT(safe_path("/x", "", r, sizeof r) == -1);
  ASSERT(safe_path("/x", "/", r, sizeof r) == -1);
  ASSERT(safe_path("/x", NULL, r, sizeof r) == -1);
  ASSERT(safe_path(NULL, R, r, sizeof r) == -1);
  ASSERT(safe_path("/x", R, r, 0) == -1);
  /* resolved result is always root + '/' boundary */
  ASSERT(safe_path("/wwwx", R, r, sizeof r) == 0); ASSERT_STR_EQ(r, "/w/www/wwwx");
}

TEST(truncation_rejected) {
  char r[10];
  ASSERT(safe_path("/assets/app.js", R, r, sizeof r) == -1);
  ASSERT(safe_path("/a", R, r, 8) == -1);   /* "/w/www/a" needs 9 */
  ASSERT(safe_path("/a", R, r, 9) == 0);
}

TEST(maps_api_to_state) { http_req q;
  ASSERT(http_parse_request("GET /api/system HTTP/1.0\r\n\r\n", &q) == 0);
  ASSERT_STR_EQ(q.path, "/api/system"); ASSERT_STR_EQ(q.method, "GET"); ASSERT_STR_EQ(q.dpath, "/api/system"); }

TEST(parse_details) { http_req q;
  ASSERT(http_parse_request("POST /a%20b?x=1 HTTP/1.1\r\nHost: h\r\nCOOKIE:  sid=abc; z=1\r\nContent-Length: 12\r\n\r\nbody", &q) == 0);
  ASSERT_STR_EQ(q.method, "POST"); ASSERT_STR_EQ(q.path, "/a%20b"); ASSERT_STR_EQ(q.dpath, "/a b");
  ASSERT_STR_EQ(q.query, "x=1"); ASSERT_STR_EQ(q.cookie, "sid=abc; z=1"); ASSERT(q.content_length == 12);
  ASSERT(http_parse_request("GET /%00x HTTP/1.0\r\n\r\n", &q) == 0); ASSERT_STR_EQ(q.dpath, "");
  ASSERT(http_parse_request("GET / HTTP/1.0\r\n\r\n", &q) == 0); ASSERT(q.content_length == -1); ASSERT_STR_EQ(q.cookie, "");
}

TEST(parse_malformed) { http_req q;
  ASSERT(http_parse_request("", &q) == -1);
  ASSERT(http_parse_request("GET /", &q) == -1);
  ASSERT(http_parse_request("GET /\r\n\r\n", &q) == -1);
  ASSERT(http_parse_request("GET x HTTP/1.0\r\n\r\n", &q) == -1);
  ASSERT(http_parse_request("GET / SPDY/3\r\n\r\n", &q) == -1);
  ASSERT(http_parse_request(" / HTTP/1.0\r\n\r\n", &q) == -1);
  ASSERT(http_parse_request("GET / HTTP/1.0\r\nContent-Length: abc\r\n\r\n", &q) == -1);
  ASSERT(http_parse_request(NULL, &q) == -1);
  { char big[2000]; memset(big, 'a', sizeof big); memcpy(big, "GET /", 5);
    strcpy(big + 1500, " HTTP/1.0\r\n\r\n"); ASSERT(http_parse_request(big, &q) == -1); }
}

/* ---- serving ---- */
static char base[256];
static void wf(const char *rel, const char *txt) { char p[512]; FILE *f; snprintf(p, sizeof p, "%s/%s", base, rel);
  f = fopen(p, "w"); if (f) { fputs(txt, f); fclose(f); } }
static void setup(void) { char c[600];
  snprintf(base, sizeof base, "/tmp/uisrv_test_%d", (int)getpid());
  snprintf(c, sizeof c, "rm -rf %s; mkdir -p %s/www/assets %s/state %s/secret", base, base, base, base); system(c);
  wf("www/index.html", "<h1>hi</h1>"); wf("www/assets/app.js", "var a=1;"); wf("www/assets/s.css", "a{}");
  wf("state/system.json", "{\"ok\":1}"); wf("secret/system.json", "SECRET"); wf("www/../wwwx", "SECRET");
  snprintf(c, sizeof c, "ln -s %s/secret %s/www/link; ln -s %s/secret/system.json %s/state/evil.json", base, base, base, base); system(c); }
static int res(const char *raw, const char **ct, char *file) { http_req q; http_parse_request(raw, &q); return ui_resolve(base, &q, file, 512, ct); }

TEST(serving_resolve) { char f[512]; const char *ct = "";
  setup();
  ASSERT(res("GET / HTTP/1.0\r\n\r\n", &ct, f) == 200); ASSERT(strstr(ct, "text/html")); ASSERT(strstr(f, "www/index.html"));
  ASSERT(res("GET /assets/app.js HTTP/1.0\r\n\r\n", &ct, f) == 200); ASSERT_STR_EQ(ct, "application/javascript");
  ASSERT(res("GET /assets/s.css HTTP/1.0\r\n\r\n", &ct, f) == 200); ASSERT_STR_EQ(ct, "text/css");
  ASSERT(res("GET /api/system HTTP/1.0\r\n\r\n", &ct, f) == 200); ASSERT_STR_EQ(ct, "application/json"); ASSERT(strstr(f, "state/system.json"));
  ASSERT(res("GET /api/nope HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /nope.js HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /assets HTTP/1.0\r\n\r\n", &ct, f) == 404);   /* directory */
  ASSERT(res("GET /assets/ HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("POST / HTTP/1.0\r\n\r\n", &ct, f) == 405);
}

TEST(serving_attacks) { char f[512]; const char *ct;
  ASSERT(res("GET /../wwwx HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /..%2fwwwx HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /%2e%2e/secret/system.json HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /link/system.json HTTP/1.0\r\n\r\n", &ct, f) == 404);       /* symlink dir escape */
  ASSERT(res("GET /api/evil HTTP/1.0\r\n\r\n", &ct, f) == 404);               /* symlink file escape */
  ASSERT(res("GET /api/../system HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /api/%2e%2e%2fsecret%2fsystem HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /api/system%00 HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /api/sys/tem HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /api/ HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET /api/system.json HTTP/1.0\r\n\r\n", &ct, f) == 404);
  ASSERT(res("GET //etc/passwd HTTP/1.0\r\n\r\n", &ct, f) == 404);
}

TEST(serving_response) { http_req q; char buf[2048]; FILE *t = tmpfile(); size_t n;
  http_parse_request("GET /api/system HTTP/1.0\r\n\r\n", &q);
  ASSERT(ui_serve(base, &q, fileno(t)) == 200);
  rewind(t); n = fread(buf, 1, sizeof buf - 1, t); buf[n] = 0;
  ASSERT(strncmp(buf, "HTTP/1.0 200 OK", 15) == 0); ASSERT(strstr(buf, "Content-Type: application/json"));
  ASSERT(strstr(buf, "Content-Length: 8")); ASSERT(strstr(buf, "\r\n\r\n{\"ok\":1}"));
  fclose(t); t = tmpfile();
  http_parse_request("GET /../../etc/passwd HTTP/1.0\r\n\r\n", &q);
  ASSERT(ui_serve(base, &q, fileno(t)) == 404);
  rewind(t); n = fread(buf, 1, sizeof buf - 1, t); buf[n] = 0;
  ASSERT(strncmp(buf, "HTTP/1.0 404", 12) == 0); ASSERT(!strstr(buf, "root:"));
  fclose(t);
  { char c[300]; snprintf(c, sizeof c, "rm -rf %s", base); system(c); }
}
