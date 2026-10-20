#include "parse_sessions.h"
#include "json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAXN 64
#define FLD 128
#define PATHF 512

typedef struct { char pid[16], user[FLD], machine[FLD], ip[FLD], share[FLD]; } sess;
typedef struct { char pid[16], machine[FLD], share[FLD]; } tree;
typedef struct { char user[FLD], path[PATHF]; } ofile;
typedef struct { char user[FLD], ip[FLD]; } sshs;

enum { S_NONE, S_SESS, S_TREE, S_LOCK };

static void copyn(char *d, size_t dn, const char *s, size_t n) {
  if (n >= dn) n = dn - 1;
  memcpy(d, s, n);
  d[n] = '\0';
}

/* Next whitespace-delimited token; advances *p. Returns length (0 = none). */
static size_t tok(const char **p, const char **start) {
  const char *s = *p;
  size_t n = 0;
  while (*s == ' ' || *s == '\t') s++;
  *start = s;
  while (s[n] && s[n] != ' ' && s[n] != '\t') n++;
  *p = s + n;
  return n;
}

static int starts(const char *l, const char *pfx) { return strncmp(l, pfx, strlen(pfx)) == 0; }

/* "host (ipv4:1.2.3.4:5678)" machine token -> 1.2.3.4 ; otherwise token as-is. */
static void extract_ip(const char *p, char *ip, size_t n) {
  const char *start, *q;
  size_t len = tok(&p, &start);
  ip[0] = '\0';
  if (!len) return;
  copyn(ip, n, start, len);
  /* look at the optional parenthesised "(ipv4:addr:port)" that follows */
  q = p;
  while (*q == ' ' || *q == '\t') q++;
  if (*q == '(' && (starts(q, "(ipv4:") || starts(q, "(ipv6:"))) {
    const char *a = q + 6, *e = a;
    if (starts(q, "(ipv6:") && *a == '[') { a++; e = a; while (*e && *e != ']' && *e != ')') e++; }
    else { while (*e && *e != ':' && *e != ')') e++; }
    if (e > a) copyn(ip, n, a, (size_t)(e - a));
  }
}

static int is_dow(const char *s, size_t n) {
  static const char *d[] = { "Mon","Tue","Wed","Thu","Fri","Sat","Sun" };
  int i;
  if (n != 3) return 0;
  for (i = 0; i < 7; i++) if (!strncmp(s, d[i], 3)) return 1;
  return 0;
}

static void join_path(char *dst, size_t dn, const char *share, const char *name) {
  size_t sl = strlen(share);
  if (!sl) snprintf(dst, dn, "%s", name);
  else if (share[sl - 1] == '/' || name[0] == '/') snprintf(dst, dn, "%s%s", share, name);
  else snprintf(dst, dn, "%s/%s", share, name);
}

static int parse_smb(const char *s, sess *ss, int *ns, tree *tt, int *nt, ofile *of, int *nf) {
  int state = S_NONE;
  while (s && *s) {
    const char *eol = strchr(s, '\n');
    size_t ll = eol ? (size_t)(eol - s) : strlen(s);
    char line[1024];
    const char *p, *a;
    size_t n;

    copyn(line, sizeof line, s, ll);
    s += ll + (eol ? 1 : 0);
    if (line[0] && line[strlen(line) - 1] == '\r') line[strlen(line) - 1] = '\0';

    p = line;
    n = tok(&p, &a);
    if (!n) { if (state == S_SESS || state == S_TREE) state = S_NONE; continue; } /* blank */
    if (line[0] == '-') continue;                                                   /* rule */
    if (starts(a, "Samba version")) { state = S_NONE; continue; }
    if (starts(a, "PID") && strstr(line, "Username")) { state = S_SESS; continue; }
    if (starts(a, "Service") && strstr(line, "pid")) { state = S_TREE; continue; }
    if (starts(a, "Locked") ) { state = S_LOCK; continue; }
    if (starts(a, "Pid") && strstr(line, "DenyMode")) { state = S_LOCK; continue; }
    if (starts(a, "No locked")) { state = S_NONE; continue; }

    if (state == S_SESS && *ns < MAXN) {
      sess *x = &ss[*ns];
      const char *u, *g, *m;
      memset(x, 0, sizeof *x);
      copyn(x->pid, sizeof x->pid, a, n);
      if (!tok(&p, &u)) continue;
      { size_t un = 0; while (u[un] && u[un] != ' ' && u[un] != '\t') un++; copyn(x->user, FLD, u, un); }
      if (!tok(&p, &g)) continue;                 /* group */
      if (!tok(&p, &m)) continue;                 /* machine */
      extract_ip(m, x->ip, FLD);
      { size_t mn = 0; while (m[mn] && m[mn] != ' ' && m[mn] != '\t') mn++; copyn(x->machine, FLD, m, mn); }
      (*ns)++;
    } else if (state == S_TREE && *nt < MAXN) {
      tree *t = &tt[*nt];
      const char *pid, *mach;
      size_t pn, mn;
      memset(t, 0, sizeof *t);
      copyn(t->share, FLD, a, n);
      pn = tok(&p, &pid);
      mn = tok(&p, &mach);
      if (!pn || !mn) continue;
      copyn(t->pid, sizeof t->pid, pid, pn);
      copyn(t->machine, FLD, mach, mn);
      (*nt)++;
    } else if (state == S_LOCK && *nf < MAXN) {
      ofile *f = &of[*nf];
      const char *tk, *rest;
      char sharep[PATHF], name[PATHF];
      size_t tn;
      int i;
      memset(f, 0, sizeof *f);
      /* a = pid ; user ; deny ; access ; r/w ; oplock ; sharepath ; name... */
      if (!(tn = tok(&p, &tk))) continue;
      copyn(f->user, FLD, tk, tn);
      for (i = 0; i < 4; i++) if (!tok(&p, &tk)) break;     /* deny, access, rw, oplock */
      if (i < 4) { f->user[0] = '\0'; continue; }
      if (!(tn = tok(&p, &tk))) { f->user[0] = '\0'; continue; }
      copyn(sharep, sizeof sharep, tk, tn);
      while (*p == ' ' || *p == '\t') p++;
      rest = p;
      /* name runs to end of line, minus a trailing "Dow Mon DD HH:MM:SS YYYY" time */
      {
        const char *q = rest, *cut = rest + strlen(rest);
        while (*q) {
          const char *st;
          const char *r = q;
          size_t k = tok(&r, &st);
          if (!k) break;
          if (st != rest && is_dow(st, k)) { cut = st; break; }
          q = r;
        }
        while (cut > rest && (cut[-1] == ' ' || cut[-1] == '\t')) cut--;
        copyn(name, sizeof name, rest, (size_t)(cut - rest));
      }
      join_path(f->path, sizeof f->path, sharep, name);
      (*nf)++;
    }
  }
  return 0;
}

static void parse_who(const char *s, sshs *w, int *nw) {
  while (s && *s && *nw < MAXN) {
    const char *eol = strchr(s, '\n');
    size_t ll = eol ? (size_t)(eol - s) : strlen(s);
    char line[512];
    const char *p, *a, *lp, *rp;
    size_t n;

    copyn(line, sizeof line, s, ll);
    s += ll + (eol ? 1 : 0);
    p = line;
    n = tok(&p, &a);
    if (!n) continue;
    memset(&w[*nw], 0, sizeof w[*nw]);
    copyn(w[*nw].user, FLD, a, n);
    lp = strrchr(line, '(');
    rp = lp ? strchr(lp, ')') : NULL;
    if (lp && rp && rp > lp + 1) copyn(w[*nw].ip, FLD, lp + 1, (size_t)(rp - lp - 1));
    (*nw)++;
  }
}

int build_sessions_json(const char *smbstatus_out, const char *who_out, char *out, size_t outlen) {
  sess ss[MAXN]; tree tt[MAXN]; ofile of[MAXN]; sshs w[MAXN];
  int ns = 0, nt = 0, nf = 0, nw = 0, i, j, rc;
  jb b;

  if (!out || outlen == 0) return -1;
  parse_smb(smbstatus_out, ss, &ns, tt, &nt, of, &nf);
  parse_who(who_out, w, &nw);

  for (i = 0; i < ns; i++) {
    ss[i].share[0] = '\0';
    for (j = 0; j < nt; j++)
      if (!strcmp(tt[j].pid, ss[i].pid)) { snprintf(ss[i].share, FLD, "%s", tt[j].share); break; }
    if (!ss[i].share[0])
      for (j = 0; j < nt; j++)
        if (!strcmp(tt[j].machine, ss[i].machine) || !strcmp(tt[j].machine, ss[i].ip)) {
          snprintf(ss[i].share, FLD, "%s", tt[j].share); break;
        }
  }

  jb_init(&b);
  jb_raw(&b, "{\"smb\":[");
  for (i = 0; i < ns; i++) {
    if (i) jb_raw(&b, ",");
    jb_raw(&b, "{"); jb_kstr(&b, "user", ss[i].user); jb_kstr(&b, "ip", ss[i].ip);
    jb_kstr(&b, "share", ss[i].share); jb_raw(&b, "}");
  }
  jb_raw(&b, "],\"open_files\":[");
  for (i = 0; i < nf; i++) {
    if (i) jb_raw(&b, ",");
    jb_raw(&b, "{"); jb_kstr(&b, "user", of[i].user); jb_kstr(&b, "path", of[i].path); jb_raw(&b, "}");
  }
  jb_raw(&b, "],\"ssh\":[");
  for (i = 0; i < nw; i++) {
    if (i) jb_raw(&b, ",");
    jb_raw(&b, "{"); jb_kstr(&b, "user", w[i].user); jb_kstr(&b, "ip", w[i].ip); jb_raw(&b, "}");
  }
  jb_raw(&b, "]}");

  if (!b.buf || b.len + 1 > outlen) rc = -1;
  else { memcpy(out, b.buf, b.len + 1); rc = (int)b.len; }
  jb_free(&b);
  return rc;
}
