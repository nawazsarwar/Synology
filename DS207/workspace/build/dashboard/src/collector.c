/* DS207 dashboard collector: periodically snapshots system state to JSON files.
 * collect_once() is pure orchestration over (a) /proc files under
 * $COLLECTOR_PROC_DIR and (b) a tool_provider; only collector_exec_tool()
 * touches fork/exec, so everything else is host-testable. */
#define _POSIX_C_SOURCE 200809L
#include "collector.h"
#include "parse_system.h"
#include "parse_net.h"
#include "parse_wan.h"
#include "parse_diskio.h"
#include "parse_storage.h"
#include "parse_smart.h"
#include "parse_services.h"
#include "parse_sessions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#define MAXIN  (64 * 1024)
#define MAXOUT (64 * 1024)
#define TOOL_TIMEOUT_MS 10000   /* a hung tool must not stall the loop */

/* ---- thin exec wrapper + whitelist ------------------------------------ */
static const char *ui_dir(void) {
  const char *u = getenv("COLLECTOR_UI_DIR");   /* operator config, not request data */
  return (u && *u) ? u : "/usr/syno";
}

char *collector_exec_tool(tool_id id) {
  char smbstatus[256], conf[256], smbd[256], dbear[256];
  char *argv[6]; int fd[2]; pid_t pid; char *buf; size_t len = 0; ssize_t r; int st;
  if (snprintf(smbstatus, sizeof smbstatus, "%s/bin/smbstatus", ui_dir()) >= (int)sizeof smbstatus ||
      snprintf(conf, sizeof conf, "%s/etc/smb.conf", ui_dir()) >= (int)sizeof conf ||
      snprintf(smbd, sizeof smbd, "%s/sbin/smbd", ui_dir()) >= (int)sizeof smbd ||
      snprintf(dbear, sizeof dbear, "%s/ssh/dropbearmulti", ui_dir()) >= (int)sizeof dbear)
    return NULL;   /* overlong COLLECTOR_UI_DIR: fail loudly, don't truncate */
  switch (id) {   /* THE whitelist: fixed argv only */
    case TOOL_DF:        argv[0]="df"; argv[1]=NULL; break;
    case TOOL_SMART:     argv[0]="/usr/syno/bin/smartctl"; argv[1]="-a"; argv[2]="/dev/sda"; argv[3]=NULL; break;
    case TOOL_SMBSTATUS: argv[0]=smbstatus; argv[1]="-s"; argv[2]=conf; argv[3]=NULL; break;
    case TOOL_WHO:       argv[0]="who"; argv[1]=NULL; break;
    case TOOL_NETSTAT:   argv[0]="netstat"; argv[1]="-ltn"; argv[2]=NULL; break;
    case TOOL_PS:        argv[0]="ps"; argv[1]="w"; argv[2]=NULL; break;
    case TOOL_VER_SMBD:  argv[0]=smbd; argv[1]="-V"; argv[2]=NULL; break;
    /* our Dropbear ships as the multi-call binary; invoke the dropbear applet */
    case TOOL_VER_SSH:   argv[0]=dbear; argv[1]="dropbear"; argv[2]="-V"; argv[3]=NULL; break;
    default: return NULL;
  }
  if (pipe(fd) != 0) return NULL;
  pid = fork();
  if (pid < 0) { close(fd[0]); close(fd[1]); return NULL; }
  if (pid == 0) {
    setpgid(0, 0);   /* own group so a timeout kill reaches grandchildren */
    int nul = open("/dev/null", O_RDONLY);
    if (nul >= 0) dup2(nul, 0);
    dup2(fd[1], 1); dup2(fd[1], 2);   /* some -V tools print to stderr */
    close(fd[0]); close(fd[1]);
    execvp(argv[0], argv);
    _exit(127);
  }
  close(fd[1]);
  buf = malloc(MAXIN + 1);
  if (buf) {
    struct timespec t0, t1; long left = TOOL_TIMEOUT_MS; int timed_out = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (len < MAXIN) {
      struct pollfd pf; int pr;
      pf.fd = fd[0]; pf.events = POLLIN; pf.revents = 0;
      pr = poll(&pf, 1, (int)left);
      if (pr == 0) { timed_out = 1; break; }
      if (pr > 0) {
        r = read(fd[0], buf + len, MAXIN - len);
        if (r == 0) break;
        if (r > 0) len += (size_t)r;
        else if (errno != EINTR) break;
      } else if (errno != EINTR) break;
      clock_gettime(CLOCK_MONOTONIC, &t1);   /* total deadline, not per-read */
      left = TOOL_TIMEOUT_MS - ((long)(t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000);
      if (left <= 0) { timed_out = 1; break; }
    }
    buf[len] = 0;
    if (timed_out) kill(-pid, SIGKILL), kill(pid, SIGKILL);
  } else {
    kill(-pid, SIGKILL), kill(pid, SIGKILL);
  }
  close(fd[0]);
  while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
  if (buf && len == 0) { free(buf); buf = NULL; }
  return buf;
}

static tool_provider g_provider = NULL;
void collector_set_provider(tool_provider p) { g_provider = p; }
static char *run_tool(tool_id id) { return (g_provider ? g_provider : collector_exec_tool)(id); }

/* ---- helpers ----------------------------------------------------------- */
static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb"); char *b; size_t n;
  if (!f) return NULL;
  b = malloc(MAXIN + 1);
  if (!b) { fclose(f); return NULL; }
  n = fread(b, 1, MAXIN, f); b[n] = 0; fclose(f);
  return b;
}
static char *read_proc(const char *rel) {
  const char *root = getenv("COLLECTOR_PROC_DIR");
  char p[512];
  snprintf(p, sizeof p, "%s/%s", (root && *root) ? root : "/proc", rel);
  return read_file(p);
}

/* write statedir/name.json.tmp then rename over name.json */
static int write_atomic(const char *dir, const char *name, const char *data, size_t n) {
  char tmp[512], fin[512]; FILE *f;
  if (snprintf(fin, sizeof fin, "%s/%s.json", dir, name) >= (int)sizeof fin ||
      snprintf(tmp, sizeof tmp, "%s/%s.json.tmp", dir, name) >= (int)sizeof tmp)
    return -1;   /* overlong statedir */
  f = fopen(tmp, "wb");
  if (!f) return -1;
  if (fwrite(data, 1, n, f) != n || fflush(f) != 0 || fsync(fileno(f)) != 0) {
    fclose(f); unlink(tmp); return -1;
  }
  if (fclose(f) != 0) { unlink(tmp); return -1; }
  if (rename(tmp, fin) != 0) { unlink(tmp); return -1; }
  return 0;
}

static const char *nz(const char *s) { return s ? s : ""; }

/* ---- per-snapshot collectors (each returns 0 ok / 1 failure) ---------- */
static int snap_system(const char *dir, char *out) {
  char *a = read_proc("stat"), *b = read_proc("meminfo"), *c = read_proc("loadavg"), *d = read_proc("uptime");
  int n = build_system_json(nz(a), nz(b), nz(c), nz(d), out, MAXOUT);
  free(a); free(b); free(c); free(d);
  return n < 0 || write_atomic(dir, "system", out, (size_t)n);
}
static int snap_simple(const char *dir, const char *name, const char *rel, int kind, char *out) {
  char *a = read_proc(rel); int n;
  n = kind == 0 ? build_net_json(nz(a), out, MAXOUT) : build_diskio_json(nz(a), out, MAXOUT);
  free(a);
  return n < 0 || write_atomic(dir, name, out, (size_t)n);
}
static int snap_wan(const char *dir, char *out) {
  char *a = read_proc("net/tcp");
  int n = build_wan_json(nz(a), out, MAXOUT);
  free(a);
  return n < 0 || write_atomic(dir, "wan", out, (size_t)n);
}
static int snap_storage(const char *dir, char *out) {
  char *df = run_tool(TOOL_DF), *md = read_proc("mdstat");
  int n = build_storage_json(nz(df), nz(md), out, MAXOUT);
  free(df); free(md);
  return n < 0 || write_atomic(dir, "storage", out, (size_t)n);
}
static int snap_disks(const char *dir, char *out) {
  char *sm = run_tool(TOOL_SMART); int n;
  out[0] = '[';
  /* build_smart_json returns bytes written or -1 (never a would-be length) */
  n = build_smart_json("sda", nz(sm), out + 1, MAXOUT - 2);
  free(sm);
  if (n < 0 || n > MAXOUT - 3) {   /* failure: safe fallback, still report failure */
    write_atomic(dir, "disks", "[]", 2);
    return 1;
  }
  out[n + 1] = ']'; out[n + 2] = 0;
  return write_atomic(dir, "disks", out, (size_t)n + 2);
}
static int snap_sessions(const char *dir, char *out) {
  char *s = run_tool(TOOL_SMBSTATUS), *w = run_tool(TOOL_WHO);
  int n = build_sessions_json(nz(s), nz(w), out, MAXOUT);
  free(s); free(w);
  return n < 0 || write_atomic(dir, "sessions", out, (size_t)n);
}
static int snap_services(const char *dir, char *out) {
  static const struct { const char *name; const char *proc; int port; tool_id ver; } S[] = {
    { "smbd", "smbd", 445, TOOL_VER_SMBD }, { "nmbd", "nmbd", 139, TOOL_COUNT },
    { "afpd", "afpd", 548, TOOL_COUNT },    { "ssh", "dropbear", 2222, TOOL_VER_SSH },
  };
  enum { N = 4 };
  svc_probe p[N]; char *ver[N]; int i, n;
  char *ps = run_tool(TOOL_PS), *ns = run_tool(TOOL_NETSTAT);
  for (i = 0; i < N; i++) {
    char port[16]; int up;
    snprintf(port, sizeof port, ":%d ", S[i].port);
    up = (ps && strstr(ps, S[i].proc)) || (ns && strstr(ns, port));
    ver[i] = (up && S[i].ver != TOOL_COUNT) ? run_tool(S[i].ver) : NULL;
    if (ver[i]) { char *e = strpbrk(ver[i], "\r\n"); if (e) *e = 0; if (!*ver[i]) { free(ver[i]); ver[i] = NULL; } }
    p[i].name = S[i].name; p[i].up = up; p[i].version = ver[i]; p[i].port = S[i].port;
  }
  /* nmbd is part of the same Samba build as smbd: share its version string
     (aliases ver[0]; freed once below, never via p[1]). */
  if (N > 1 && p[1].up && !p[1].version && p[0].version) p[1].version = p[0].version;
  n = build_services_json(p, N, out, MAXOUT);
  for (i = 0; i < N; i++) free(ver[i]);
  free(ps); free(ns);
  return n < 0 || write_atomic(dir, "services", out, (size_t)n);
}

static int do_light(const char *d, char *o) {
  return snap_system(d, o) + snap_simple(d, "net", "net/dev", 0, o)
       + snap_simple(d, "diskio", "diskstats", 1, o) + snap_storage(d, o)
       + snap_wan(d, o);
}

int collect_once(const char *statedir) {
  char *out = malloc(MAXOUT); int f;
  if (!out || !statedir) { free(out); return 7; }
  f = do_light(statedir, out) + snap_disks(statedir, out)
    + snap_services(statedir, out) + snap_sessions(statedir, out);
  free(out);
  return f;
}

#ifndef COLLECTOR_NO_MAIN
static volatile sig_atomic_t g_stop = 0;
static void on_sig(int s) { (void)s; g_stop = 1; }

static int loop_run(const char *dir) {
  char *out = malloc(MAXOUT); unsigned long t = 0;
  if (!out) return 1;
  signal(SIGTERM, on_sig); signal(SIGINT, on_sig);
  while (!g_stop) {                 /* 1s base tick */
    if (t % 300 == 0) snap_disks(dir, out);
    if (t % 60 == 0)  snap_services(dir, out);
    if (t % 5 == 0)   snap_sessions(dir, out);
    if (t % 2 == 0)   do_light(dir, out);
    t++; sleep(1);
  }
  free(out);
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 3 && strcmp(argv[1], "--once") == 0) return collect_once(argv[2]) ? 1 : 0;
  if (argc == 3 && strcmp(argv[1], "--loop") == 0) return loop_run(argv[2]);
  fprintf(stderr, "usage: collector --once|--loop <statedir>\n");
  return 2;
}
#endif
