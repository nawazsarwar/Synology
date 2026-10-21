#include "test.h"
#include "parse_sessions.h"

static const char FX_SMB[] =
#include "fixtures/smbstatus.txt"
;
static const char FX_EMPTY[] =
#include "fixtures/smbstatus_empty.txt"
;
static const char FX_WHO[] =
#include "fixtures/who.txt"
;

TEST(sessions_populated_real_capture) {
  char out[2048];
  int n = build_sessions_json(FX_SMB, FX_WHO, out, sizeof out);
  ASSERT(n > 0 && n == (int)strlen(out));
  ASSERT(strstr(out, "{\"user\":\"admin\",\"ip\":\"192.168.1.5\",\"share\":\"smbtest\"}"));
  /* locked-files uid 1024 resolved to the session user by pid */
  ASSERT(strstr(out, "{\"user\":\"admin\",\"path\":\"/volume1/smbtest/ss_probe.txt\"}"));
  ASSERT(strstr(out, "\"ssh\":[{\"user\":\"root\",\"ip\":\"192.168.1.5\"}]"));
}

TEST(sessions_multi_ipv6_and_state_guards) {
  char out[2048];
  const char *s =
    "PID     Username     Group        Machine                                   Protocol Version  Encryption           Signing\n"
    "-----\n"
    "10    nawaz        users        192.168.1.50 (ipv4:192.168.1.50:51234)    SMB3_11           -   -\n"
    "11    guest        nogroup      macbook (ipv6:fe80::1c2:3d4:5:60357)      SMB2_10           -   -\n"
    "\n"
    "Service      pid     Machine       Connected at                     Encryption   Signing\n"
    "-----\n"
    "Locked       10      192.168.1.50  Tue Oct 21 10:00:00 2026         -            -\n"
    "No           11      macbook       Tue Oct 21 10:05:12 2026         -            -\n"
    "\n"
    "Locked files:\n"
    "Pid          User(ID)   DenyMode   Access      R/W        Oplock           SharePath   Name   Time\n"
    "-----\n"
    "10   1000   DENY_NONE  0x80  RDONLY  NONE  /volume1/Locked   a.txt   Tue Oct 21 10:01:30 2026\n"
    "99   1001   DENY_NONE  0x80  RDONLY  NONE  /volume1/x   b.txt   Tue Oct 21 10:01:30 2026\n";
  ASSERT(build_sessions_json(s, NULL, out, sizeof out) > 0);
  ASSERT(strstr(out, "{\"user\":\"nawaz\",\"ip\":\"192.168.1.50\",\"share\":\"Locked\"}"));
  ASSERT(strstr(out, "{\"user\":\"guest\",\"ip\":\"fe80::1c2:3d4:5\",\"share\":\"No\"}"));
  ASSERT(strstr(out, "{\"user\":\"nawaz\",\"path\":\"/volume1/Locked/a.txt\"}"));
  ASSERT(strstr(out, "{\"user\":\"1001\",\"path\":\"/volume1/x/b.txt\"}"));  /* unknown pid keeps uid */
}

TEST(sessions_who_console_and_procps) {
  char out[512];
  const char *w = "USER       TTY      IDLE      TIME            HOST\n"
                  "root       console  00:01     Feb  8 23:05:18\n"
                  "LOGIN      console  old       Nov 11 08:58:19\n"
                  "root       pts/1    00:02     Feb  8 23:06:00 fe80::1\n"
                  "bob        pts/2    2026-10-21 09:58 (10.0.0.7)\n";
  ASSERT(build_sessions_json(NULL, w, out, sizeof out) > 0);
  ASSERT_STR_EQ(out, "{\"smb\":[],\"open_files\":[],\"ssh\":[{\"user\":\"root\",\"ip\":\"fe80::1\"},{\"user\":\"bob\",\"ip\":\"10.0.0.7\"}]}");
}

TEST(sessions_empty_smbstatus) {
  char out[512];
  ASSERT(build_sessions_json(FX_EMPTY, FX_WHO, out, sizeof out) > 0);
  ASSERT_STR_EQ(out, "{\"smb\":[],\"open_files\":[],\"ssh\":[{\"user\":\"root\",\"ip\":\"192.168.1.5\"}]}");
}

TEST(sessions_null_and_blank_inputs) {
  char out[128];
  ASSERT(build_sessions_json(NULL, NULL, out, sizeof out) > 0);
  ASSERT_STR_EQ(out, "{\"smb\":[],\"open_files\":[],\"ssh\":[]}");
  ASSERT(build_sessions_json("", "\n\n", out, sizeof out) > 0);
  ASSERT_STR_EQ(out, "{\"smb\":[],\"open_files\":[],\"ssh\":[]}");
  ASSERT(build_sessions_json("garbage\nPID Username\n99\n", "x", out, sizeof out) > 0);
}

TEST(sessions_no_tree_match_empty_share_and_overflow) {
  char out[256], tiny[8];
  const char *s = "PID Username Group Machine\n---\n7 bob grp 10.0.0.9 SMB2\n";
  ASSERT(build_sessions_json(s, NULL, out, sizeof out) > 0);
  ASSERT(strstr(out, "{\"user\":\"bob\",\"ip\":\"10.0.0.9\",\"share\":\"\"}"));
  ASSERT(build_sessions_json(s, NULL, tiny, sizeof tiny) == -1);
  ASSERT(build_sessions_json(s, NULL, NULL, 10) == -1);
}
