#include "test.h"
#include "parse_diskio.h"

static const char FX_DISK[] =
#include "fixtures/proc_diskstats.txt"
;

TEST(diskio_whole_disks_only) {
  char out[512];
  int n = build_diskio_json(FX_DISK, out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"dev\":\"sda\""));
  ASSERT(strstr(out, "\"rd_sectors\":2048000"));
  ASSERT(strstr(out, "\"wr_sectors\":1024000"));
  ASSERT(strstr(out, "\"dev\":\"md2\""));
  ASSERT(strstr(out, "\"rd_sectors\":999999"));
  ASSERT(strstr(out, "\"wr_sectors\":888888"));
  ASSERT(!strstr(out, "ram0"));
  ASSERT(!strstr(out, "sda5"));
  ASSERT(!strstr(out, "sda1"));
}

TEST(diskio_short_input_is_empty_array) {
  char out[64];
  ASSERT(build_diskio_json("", out, sizeof out) == 2);
  ASSERT_STR_EQ(out, "[]");
  ASSERT(build_diskio_json("   8    0 sda 1 2\n", out, sizeof out) == 2);
  ASSERT(build_diskio_json(NULL, out, sizeof out) == 2);
}

TEST(diskio_overflow_returns_minus_one) {
  char out[8];
  ASSERT(build_diskio_json(FX_DISK, out, sizeof out) == -1);
}
