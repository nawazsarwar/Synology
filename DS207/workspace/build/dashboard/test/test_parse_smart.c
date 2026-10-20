#include "test.h"
#include "parse_smart.h"

static const char FX_SMART[] =
#include "fixtures/smartctl_a.txt"
;
static const char FX_SMART_TEMP[] =
#include "fixtures/smartctl_temp.txt"
;

TEST(smart_real_ssd_no_temperature) {
  char out[1024];
  int n = build_smart_json("sda", FX_SMART, out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"dev\":\"sda\""));
  ASSERT(strstr(out, "SAMSUNG SSD PM830"));
  ASSERT(strstr(out, "\"serial\":\"S0X0NSAD309565\""));
  ASSERT(strstr(out, "\"health\":\"ok\""));
  ASSERT(strstr(out, "\"temp_c\":-1"));
}

TEST(smart_temperature_present) {
  char out[1024];
  int n = build_smart_json("sdb", FX_SMART_TEMP, out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"model\":\"TESTDISK 500GB\""));
  ASSERT(strstr(out, "\"health\":\"ok\""));
  ASSERT(strstr(out, "\"temp_c\":38}"));
}

TEST(smart_failed_health) {
  char out[512];
  int n = build_smart_json("sda", "SMART overall-health self-assessment test result: FAILED!\n", out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"health\":\"fail\""));
}

TEST(smart_malformed_and_overflow) {
  char out[512], tiny[8];
  int n = build_smart_json("sda", "garbage", out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"health\":\"unknown\""));
  ASSERT(strstr(out, "\"model\":\"\""));
  ASSERT(strstr(out, "\"temp_c\":-1"));
  ASSERT(build_smart_json("sda", NULL, out, sizeof out) > 0);
  ASSERT(build_smart_json("sda", FX_SMART, tiny, sizeof tiny) == -1);
  ASSERT(build_smart_json("sda", FX_SMART, NULL, 10) == -1);
}
