#include "test.h"
#include "parse_storage.h"

static const char FX_DF[] =
#include "fixtures/df.txt"
;
static const char FX_MDSTAT[] =
#include "fixtures/proc_mdstat.txt"
;

TEST(storage_flags_degraded) {
  char o[2048];
  ASSERT(build_storage_json(FX_DF, FX_MDSTAT, o, sizeof o) > 0);
  ASSERT(strstr(o, "{\"name\":\"md0\",\"level\":\"raid1\",\"state\":\"degraded\"}"));
  ASSERT(strstr(o, "{\"name\":\"md1\",\"level\":\"raid1\",\"state\":\"degraded\"}"));
  ASSERT(strstr(o, "{\"name\":\"md2\",\"level\":\"raid1\",\"state\":\"ok\"}"));
}

TEST(storage_parses_volumes) {
  char o[2048];
  build_storage_json(FX_DF, FX_MDSTAT, o, sizeof o);
  ASSERT(strstr(o, "\"mount\":\"/volume1\",\"size_kb\":241476032,\"used_kb\":456564,\"avail_kb\":240917068"));
  ASSERT(strstr(o, "\"mount\":\"/\",\"size_kb\":2451064,\"used_kb\":670204,\"avail_kb\":1678460"));
  ASSERT(!strstr(o, "/tmp"));
}

TEST(storage_recovering) {
  char o[1024];
  const char *m = "md0 : active raid1 sda1[2] sdb1[1]\n"
                  "      2490176 blocks [2/1] [_U]\n"
                  "      [==>..................]  recovery = 12.0% (1/2) finish=1min\n";
  build_storage_json("", m, o, sizeof o);
  ASSERT(strstr(o, "\"state\":\"recovering\""));
  ASSERT(!strstr(o, "degraded"));
}

TEST(storage_empty_input) {
  char o[128];
  ASSERT(build_storage_json(NULL, NULL, o, sizeof o) > 0);
  ASSERT_STR_EQ(o, "{\"volumes\":[],\"arrays\":[]}");
}

TEST(storage_overflow_returns_minus_one) {
  char o[16];
  ASSERT(build_storage_json(FX_DF, FX_MDSTAT, o, sizeof o) == -1);
}
