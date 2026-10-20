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

TEST(sessions_populated) {
  char out[2048];
  int n = build_sessions_json(FX_SMB, FX_WHO, out, sizeof out);
  ASSERT(n > 0 && n == (int)strlen(out));
  ASSERT(strstr(out, "{\"user\":\"nawaz\",\"ip\":\"192.168.1.50\",\"share\":\"public\"}"));
  ASSERT(strstr(out, "{\"user\":\"guest\",\"ip\":\"192.168.1.60\",\"share\":\"backup\"}"));
  ASSERT(strstr(out, "{\"user\":\"1000\",\"path\":\"/volume1/public/docs/my report.txt\"}"));
  ASSERT(strstr(out, "\"path\":\"/volume1/backup/tm/bundle.sparsebundle/band\"}"));
  ASSERT(strstr(out, "\"ssh\":[{\"user\":\"admin\",\"ip\":\"192.168.1.20\"},{\"user\":\"root\",\"ip\":\"\"}]"));
}

TEST(sessions_empty_smbstatus) {
  char out[512];
  ASSERT(build_sessions_json(FX_EMPTY, FX_WHO, out, sizeof out) > 0);
  ASSERT_STR_EQ(out, "{\"smb\":[],\"open_files\":[],\"ssh\":[{\"user\":\"admin\",\"ip\":\"192.168.1.20\"},{\"user\":\"root\",\"ip\":\"\"}]}");
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
