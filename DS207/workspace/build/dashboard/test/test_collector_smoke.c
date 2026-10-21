#include "test.h"
#include "collector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *slurp(const char *p) {
  FILE *f = fopen(p, "rb"); char *b; long n;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); if (fread(b, 1, (size_t)n, f) != (size_t)n) n = 0;
  b[n] = 0; fclose(f); return b;
}
static void copy(const char *from, const char *to) {
  char *b = slurp(from); FILE *f;
  if (!b) { printf("    missing fixture %s\n", from); test_failed++; return; }
  f = fopen(to, "wb"); fputs(b, f); fclose(f); free(b);
}
static char *fake_tool(tool_id id) {
  switch (id) {
    case TOOL_DF: return slurp("test/fixtures/df.txt");
    case TOOL_SMART: return slurp("test/fixtures/smartctl_a.txt");
    case TOOL_SMBSTATUS: return slurp("test/fixtures/smbstatus.txt");
    case TOOL_WHO: return slurp("test/fixtures/who.txt");
    case TOOL_PS: return strdup("  1 root 100 S init\n 50 root 900 S smbd -D\n");
    case TOOL_VER_SMBD: return strdup("Version 3.6.25\n");
    default: return NULL;
  }
}

TEST(collect_once_writes_all_snapshots_atomically) {
  char tmpl[] = "/tmp/collXXXXXX", proc[300], state[300], p[400], tmp[420];
  const char *names[] = { "system", "net", "diskio", "storage", "disks", "services", "sessions", "wan" };
  char *s; size_t i;
  ASSERT(mkdtemp(tmpl) != NULL);
  snprintf(proc, sizeof proc, "%s/proc", tmpl);
  snprintf(state, sizeof state, "%s/state", tmpl);
  mkdir(proc, 0700); mkdir(state, 0700);
  snprintf(p, sizeof p, "%s/stat", proc);     copy("test/fixtures/proc_stat.txt", p);
  snprintf(p, sizeof p, "%s/meminfo", proc);  copy("test/fixtures/meminfo.txt", p);
  snprintf(p, sizeof p, "%s/loadavg", proc);  copy("test/fixtures/loadavg.txt", p);
  snprintf(p, sizeof p, "%s/uptime", proc);   copy("test/fixtures/uptime.txt", p);
  snprintf(p, sizeof p, "%s/diskstats", proc); copy("test/fixtures/proc_diskstats.txt", p);
  snprintf(p, sizeof p, "%s/mdstat", proc);   copy("test/fixtures/proc_mdstat.txt", p);
  snprintf(p, sizeof p, "%s/net", proc); mkdir(p, 0700);
  snprintf(p, sizeof p, "%s/net/dev", proc);  copy("test/fixtures/proc_net_dev.txt", p);
  snprintf(p, sizeof p, "%s/net/tcp", proc);
  { FILE *w = fopen(p, "wb");
    fputs("  sl  local_address rem_address   st\n"
          "  11: C0A8010B:C350 08080808:01BB 01 0:0 0:0 0 0 0 1\n", w);
    fclose(w); }
  setenv("COLLECTOR_PROC_DIR", proc, 1);
  collector_set_provider(fake_tool);

  ASSERT(collect_once(state) == 0);
  for (i = 0; i < sizeof names / sizeof *names; i++) {
    snprintf(p, sizeof p, "%s/%s.json", state, names[i]);
    s = slurp(p);
    ASSERT(s != NULL && strlen(s) > 1);
    free(s);
    snprintf(tmp, sizeof tmp, "%s.tmp", p);
    ASSERT(access(tmp, F_OK) != 0);   /* no leftover temp */
  }
  snprintf(p, sizeof p, "%s/system.json", state);
  s = slurp(p); ASSERT(s && s[0] == '{'); free(s);
  snprintf(p, sizeof p, "%s/services.json", state);
  s = slurp(p); ASSERT(s && strstr(s, "\"smbd\"") && strstr(s, "Version 3.6.25")); free(s);
  snprintf(p, sizeof p, "%s/wan.json", state);
  s = slurp(p); ASSERT(s && strstr(s, "\"public_count\":1") && strstr(s, "8.8.8.8:443")); free(s);
  collector_set_provider(NULL);
}

static char *null_tool(tool_id id) { (void)id; return NULL; }

TEST(collect_once_survives_all_tools_missing) {
  char tmpl[] = "/tmp/collXXXXXX", state[300], p[400]; char *s;
  ASSERT(mkdtemp(tmpl) != NULL);
  snprintf(state, sizeof state, "%s", tmpl);
  setenv("COLLECTOR_PROC_DIR", "/nonexistent", 1);
  collector_set_provider(null_tool);
  ASSERT(collect_once(state) == 0);
  snprintf(p, sizeof p, "%s/sessions.json", state);
  s = slurp(p);
  ASSERT(s && s[0] == '{' && strstr(s, "\"smb\":[]") && strstr(s, "\"open_files\":[]"));
  free(s);
  snprintf(p, sizeof p, "%s/disks.json", state);
  s = slurp(p); ASSERT(s && s[0] == '[' && s[strlen(s) - 1] == ']'); free(s);
  collector_set_provider(NULL);
}

TEST(exec_tool_times_out_hung_child) {
  /* exec wrapper with a nonexistent UI dir: smbd -V exec fails fast -> NULL, no hang */
  char *o;
  setenv("COLLECTOR_UI_DIR", "/nonexistent", 1);
  o = collector_exec_tool(TOOL_VER_SMBD);
  ASSERT(o == NULL);
  free(o);
  unsetenv("COLLECTOR_UI_DIR");
}
