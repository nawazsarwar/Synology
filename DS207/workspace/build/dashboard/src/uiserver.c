/* uiserver serving logic + auth gate (no listen/main: Task 11). */
#include "uiserver.h"
#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define UI_MAX_FILE (4L * 1024 * 1024)

const char *ui_ctype(const char *path) {
  static const struct { const char *ext, *type; } t[] = {
    {"html", "text/html; charset=utf-8"}, {"js", "application/javascript"},
    {"mjs", "application/javascript"}, {"css", "text/css"},
    {"json", "application/json"}, {"svg", "image/svg+xml"}, {"png", "image/png"},
    {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"ico", "image/x-icon"},
    {"woff2", "font/woff2"}, {"woff", "font/woff"}, {"txt", "text/plain"},
    {"map", "application/json"}, {NULL, NULL}};
  const char *dot = strrchr(path, '.'), *slash = strrchr(path, '/');
  int i;
  if (dot && (!slash || dot > slash))
    for (i = 0; t[i].ext; i++) if (strcmp(dot + 1, t[i].ext) == 0) return t[i].type;
  return "application/octet-stream";
}

/* realpath(file) must lie under realpath(dir) with a '/' boundary. */
static int under_real(const char *dir, const char *file) {
  char rd[PATH_MAX], rf[PATH_MAX];
  size_t l;
  if (!realpath(dir, rd) || !realpath(file, rf)) return 0;
  l = strlen(rd);
  return strncmp(rf, rd, l) == 0 && rf[l] == '/';
}

static int api_name_ok(const char *s) {
  size_t i, l = strlen(s);
  if (l == 0 || l > 64) return 0;
  for (i = 0; i < l; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return 0;
  }
  return 1;
}

int ui_resolve(const char *base, const http_req *q, char *file, size_t n, const char **ctype) {
  char dir[PATH_MAX], tmp[PATH_MAX];
  struct stat st;
  if (!base || !q || !file || n == 0) return 404;
  if (strcmp(q->method, "GET") != 0) return 405;
  if (q->dpath[0] != '/') return 404;          /* decode failed / NUL */
  if (strncmp(q->dpath, "/api/", 5) == 0) {
    const char *name = q->dpath + 5;
    if (!api_name_ok(name)) return 404;
    if (snprintf(dir, sizeof dir, "%s/state", base) >= (int)sizeof dir) return 404;
    if (snprintf(tmp, sizeof tmp, "%s/%s.json", dir, name) >= (int)sizeof tmp) return 404;
  } else {
    if (snprintf(dir, sizeof dir, "%s/www", base) >= (int)sizeof dir) return 404;
    if (strcmp(q->dpath, "/") == 0) {
      if (snprintf(tmp, sizeof tmp, "%s/index.html", dir) >= (int)sizeof tmp) return 404;
    } else if (safe_path(q->path, dir, tmp, sizeof tmp) < 0) return 404;
  }
  if (stat(tmp, &st) < 0 || !S_ISREG(st.st_mode) || st.st_size > UI_MAX_FILE) return 404;
  if (!under_real(dir, tmp)) return 404;       /* symlink escape */
  if (strlen(tmp) >= n) return 404;
  strcpy(file, tmp);
  if (ctype) *ctype = ui_ctype(tmp);
  return 200;
}

static int wall(int fd, const char *b, size_t l) {
  while (l) { ssize_t w = write(fd, b, l); if (w <= 0) return -1; b += w; l -= (size_t)w; }
  return 0;
}

int ui_serve(const char *base, const http_req *q, int fd) {
  char file[PATH_MAX], hdr[256];
  const char *ct = NULL;
  int st = ui_resolve(base, q, file, sizeof file, &ct), hl;
  char *body = NULL; size_t len = 0;
  if (st == 200) {
    int f = open(file, O_RDONLY);
    struct stat s;
    if (f < 0 || fstat(f, &s) < 0 || !S_ISREG(s.st_mode)) { if (f >= 0) close(f); st = 404; }
    else {
      body = malloc((size_t)s.st_size + 1);
      if (body) { ssize_t r; len = 0;
        while (len < (size_t)s.st_size && (r = read(f, body + len, (size_t)s.st_size - len)) > 0) len += (size_t)r; }
      else st = 404;
      close(f);
    }
  }
  if (st != 200) {
    const char *msg = st == 405 ? "Method Not Allowed" : "Not Found";
    hl = snprintf(hdr, sizeof hdr, "HTTP/1.0 %d %s\r\nContent-Type: text/plain\r\n"
                  "Content-Length: %d\r\nConnection: close\r\n\r\n", st, msg, (int)strlen(msg));
    wall(fd, hdr, (size_t)hl); wall(fd, msg, strlen(msg));
    return st;
  }
  hl = snprintf(hdr, sizeof hdr, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
                "X-Content-Type-Options: nosniff\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
                ct, (unsigned long)len);
  wall(fd, hdr, (size_t)hl); wall(fd, body, len);
  free(body);
  return 200;
}

/* ---------------- auth gate (Task 10) ---------------- */
#define LOGIN_FAIL_MAX 10        /* consecutive failures before lockout */
#define LOGIN_LOCK_SECS 60
static int login_fails = 0;
static time_t login_last_fail = 0;

int ui_body_ok(long cl) { return cl <= UI_MAX_BODY; }

static const char LOGIN_HTML[] =
  "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
  "<title>DS207 Login</title></head><body style=\"font-family:sans-serif;max-width:20em;margin:4em auto\">"
  "<h3>DS207 Dashboard</h3><form id=f><input id=p type=password placeholder=\"password\" autofocus style=\"width:100%\">"
  "<p><button>Sign in</button> <span id=m style=\"color:#b00\"></span></p></form>"
  "<script>document.getElementById('f').onsubmit=function(e){e.preventDefault();"
  "fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/json'},"
  "body:JSON.stringify({password:document.getElementById('p').value})}).then(function(r){"
  "if(r.ok)location.href='/';else document.getElementById('m').textContent=r.status==429?'Too many attempts':'Wrong password';});};"
  "</script></body></html>";

static int send_resp(int fd, int st, const char *reason, const char *ctype, const char *extra,
                     const char *body) {
  char hdr[512]; int hl;
  hl = snprintf(hdr, sizeof hdr, "HTTP/1.0 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
                "X-Content-Type-Options: nosniff\r\nCache-Control: no-store\r\n%sConnection: close\r\n\r\n",
                st, reason, ctype, (unsigned long)strlen(body), extra ? extra : "");
  if (hl > 0 && (size_t)hl < sizeof hdr) { wall(fd, hdr, (size_t)hl); wall(fd, body, strlen(body)); }
  return st;
}

/* Extract password from JSON {"password":"..."} or form password=...; 0 ok, -1 none/too long. */
static int body_password(const char *body, size_t blen, char *out, size_t n) {
  char buf[UI_MAX_BODY + 1]; size_t o = 0; const char *p;
  if (!body || blen == 0 || blen > UI_MAX_BODY) return -1;
  memcpy(buf, body, blen); buf[blen] = '\0';
  p = buf; while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p == '{') {
    p = strstr(buf, "\"password\"");
    if (!p) return -1;
    p += 10; while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return -1;
    p++; while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return -1;
    p++;
    while (*p && *p != '"') {
      char c = *p++;
      if (c == '\\') {
        c = *p++;
        if (c == 'n') c = '\n'; else if (c == 't') c = '\t'; else if (c == 'r') c = '\r';
        else if (c == '"' || c == '\\' || c == '/') { /* literal */ }
        else return -1;                     /* \uXXXX etc. unsupported: fail closed */
      }
      if (o + 1 >= n) return -1;
      out[o++] = c;
    }
    if (*p != '"') return -1;
    out[o] = '\0';
    return 0;
  }
  /* application/x-www-form-urlencoded */
  p = buf;
  while (*p) {
    const char *e = strchr(p, '&'); size_t kl, vl; char tmp[UI_MAX_BODY + 1], *q;
    if (!e) e = p + strlen(p);
    if (strncmp(p, "password=", 9) == 0) {
      vl = (size_t)(e - (p + 9)); kl = 0; (void)kl;
      memcpy(tmp, p + 9, vl); tmp[vl] = '\0';
      for (q = tmp; *q; q++) if (*q == '+') *q = ' ';
      if (http_url_decode(tmp, out, n) < 0) return -1;
      return 0;
    }
    p = *e ? e + 1 : e;
  }
  return -1;
}

static int do_login(const char *base, const char *body, size_t blen, int fd) {
  char pw[AUTH_PW_MAX + 1], hf[PATH_MAX], *tok, ck[160];
  time_t n = auth_now();
  if (login_fails >= LOGIN_FAIL_MAX) {
    if (n - login_last_fail < LOGIN_LOCK_SECS)
      return send_resp(fd, 429, "Too Many Requests", "text/plain", "Retry-After: 60\r\n", "Too Many Requests");
    login_fails = 0;
  }
  if (snprintf(hf, sizeof hf, "%s/etc/passwd.hash", base) >= (int)sizeof hf) hf[0] = '\0';
  if (body_password(body, blen, pw, sizeof pw) == 0 && hf[0] && auth_verify(pw, hf)) {
    tok = auth_new_token();
    memset(pw, 0, sizeof pw);
    if (!tok) return send_resp(fd, 500, "Internal Server Error", "text/plain", NULL, "Internal Server Error");
    login_fails = 0;
    snprintf(ck, sizeof ck, "Set-Cookie: sid=%s; Path=/; HttpOnly; SameSite=Strict\r\n", tok);
    free(tok);
    return send_resp(fd, 200, "OK", "application/json", ck, "{\"ok\":true}");
  }
  memset(pw, 0, sizeof pw);
  login_fails++; login_last_fail = n;
  return send_resp(fd, 401, "Unauthorized", "application/json", NULL, "{\"ok\":false}");
}

int ui_handle(const char *base, const http_req *q, const char *body, size_t blen, int fd) {
  int authed;
  if (!base || !q) return send_resp(fd, 400, "Bad Request", "text/plain", NULL, "Bad Request");
  if (strcmp(q->dpath, "/api/login") == 0 && strcmp(q->method, "POST") == 0) {
    if (!ui_body_ok(q->content_length) || blen > UI_MAX_BODY)
      return send_resp(fd, 413, "Payload Too Large", "text/plain", NULL, "Payload Too Large");
    return do_login(base, body, blen, fd);
  }
  authed = auth_cookie_ok(q->cookie);
  if (!authed) {
    if (strncmp(q->dpath, "/api", 4) == 0 || strncmp(q->path, "/api", 4) == 0)
      return send_resp(fd, 401, "Unauthorized", "application/json", NULL, "{\"error\":\"unauthorized\"}");
    if (strcmp(q->method, "GET") == 0 && (strcmp(q->dpath, "/") == 0 || strcmp(q->dpath, "/index.html") == 0))
      return send_resp(fd, 200, "OK", "text/html; charset=utf-8", NULL, LOGIN_HTML);
    return send_resp(fd, 302, "Found", "text/plain", "Location: /\r\n", "Found");
  }
  return ui_serve(base, q, fd);
}

#ifdef UISERVER_MAIN
/* ---- uiserver main: SINGLE-PROCESS accept loop (auth.c session table and
 * login throttle are process-global; never fork per connection). ---- */
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define REQ_HDR_MAX 8192
#define REQ_DEADLINE_S 10   /* overall per-request budget (slowloris guard) */

static void usage(FILE *f) {
  fputs("usage: uiserver --root <ui dir> [--port 8181] [--user nobody]\n"
        "  <ui dir> contains www/ and state/\n", f);
}

static int drop_priv(const char *user, int explicit_user) {
  uid_t uid = 65534; gid_t gid = 65534;
  struct passwd *pw = getpwnam(user);
  if (pw) { uid = pw->pw_uid; gid = pw->pw_gid; }
  else if (explicit_user) {
    fprintf(stderr, "uiserver: unknown user '%s'\n", user);
    return -1;
  }
  if (setgroups(0, NULL) != 0) return -1;
  if (setgid(gid) != 0) return -1;
  if (setuid(uid) != 0) return -1;
  /* verify the drop is irreversible */
  if (getuid() == 0 || geteuid() == 0 || getgid() == 0) return -1;
  if (setuid(0) != -1) return -1;
  return 0;
}

/* Read headers (+ declared body, capped). Returns 0 ok, -1 drop connection. */
static int read_request(int fd, char *buf, size_t cap, http_req *q,
                        char **body, size_t *blen) {
  size_t len = 0, hdr_end = 0;
  time_t start = time(NULL);
  while (!hdr_end) {
    ssize_t r;
    if (time(NULL) - start >= REQ_DEADLINE_S) return -1;
    if (len >= cap - 1) return -1;
    r = recv(fd, buf + len, cap - 1 - len, 0);
    if (r <= 0) return -1;            /* EOF, error or SO_RCVTIMEO */
    len += (size_t)r; buf[len] = 0;
    { char *p = strstr(buf, "\r\n\r\n"); if (p) hdr_end = (size_t)(p - buf) + 4; }
  }
  if (http_parse_request(buf, q) != 0) return -1;
  *body = buf + hdr_end; *blen = len - hdr_end;
  if (q->content_length > 0) {
    if (!ui_body_ok(q->content_length)) { *blen = (size_t)UI_MAX_BODY + 1; return 0; }
    if (hdr_end + (size_t)q->content_length >= cap) return -1;
    while (*blen < (size_t)q->content_length) {
      ssize_t r;
      if (time(NULL) - start >= REQ_DEADLINE_S) return -1;
      r = recv(fd, buf + hdr_end + *blen, (size_t)q->content_length - *blen, 0);
      if (r <= 0) return -1;
      *blen += (size_t)r;
    }
    *blen = (size_t)q->content_length;
  } else if (!ui_body_ok(q->content_length)) { *blen = (size_t)UI_MAX_BODY + 1; }
  else *blen = 0;
  return 0;
}

int main(int argc, char **argv) {
  const char *root = NULL, *user = "nobody";
  int explicit_user = 0, port = 8181, i, ls, one = 1;
  struct sockaddr_in sa;
  static char buf[REQ_HDR_MAX + UI_MAX_BODY + 16];

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(stdout); return 0; }
    else if (!strcmp(argv[i], "--root") && i + 1 < argc) root = argv[++i];
    else if (!strcmp(argv[i], "--port") && i + 1 < argc) port = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--user") && i + 1 < argc) { user = argv[++i]; explicit_user = 1; }
    else { usage(stderr); return 2; }
  }
  if (!root || port < 1 || port > 65535) { usage(stderr); return 2; }

  signal(SIGPIPE, SIG_IGN);
  ls = socket(AF_INET, SOCK_STREAM, 0);
  if (ls < 0) { perror("socket"); return 1; }
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl(INADDR_ANY);
  sa.sin_port = htons((unsigned short)port);
  if (bind(ls, (struct sockaddr *)&sa, sizeof sa) != 0) { perror("bind"); return 1; }
  if (listen(ls, 16) != 0) { perror("listen"); return 1; }

  if (drop_priv(user, explicit_user) != 0) { fputs("uiserver: privilege drop failed\n", stderr); abort(); }

  for (;;) {
    http_req q; char *body = NULL; size_t blen = 0;
    struct timeval tv;
    int c = accept(ls, NULL, NULL);
    if (c < 0) { if (errno == EINTR) continue; usleep(100000); continue; }
    tv.tv_sec = 5; tv.tv_usec = 0;
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    memset(&q, 0, sizeof q);
    if (read_request(c, buf, sizeof buf, &q, &body, &blen) == 0)
      ui_handle(root, &q, body, blen, c);
    close(c);
  }
}
#endif
