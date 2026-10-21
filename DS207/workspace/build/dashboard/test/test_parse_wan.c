#include "test.h"
#include "parse_wan.h"

static const char FX_TCP[] =
#include "fixtures/proc_net_tcp.txt"
;

TEST(wan_counts_public_and_lan) {
  char out[1024];
  int n = build_wan_json(FX_TCP, out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"public_count\":1"));
  ASSERT(strstr(out, "\"lan_count\":1"));
  ASSERT(strstr(out, "\"remote\":\"8.8.8.8:443\""));
  ASSERT(strstr(out, "\"local\":\"192.168.1.11:50000\""));
  ASSERT(!strstr(out, "192.168.1.5"));   /* LAN peers are counted, not listed */
}

TEST(wan_ignores_listeners) {
  char out[256];
  const char *l = "  sl  local_address rem_address   st\n"
                  "   0: 00000000:0203 00000000:0000 0A 0:0 00:0 0 0 0 1\n";
  build_wan_json(l, out, sizeof out);
  ASSERT_STR_EQ(out, "{\"public_conns\":[],\"public_count\":0,\"lan_count\":0}");
}

TEST(wan_classifies_ranges) {
  char out[1024];
  const char *t =
    "1: C0A8010B:0016 0A000001:1000 01 0\n"   /* 10.0.0.1 lan */
    "2: C0A8010B:0016 AC100001:1000 01 0\n"   /* 172.16.0.1 lan */
    "3: C0A8010B:0016 AC200001:1000 01 0\n"   /* 172.32.0.1 public */
    "4: C0A8010B:0016 7F000001:1000 01 0\n"   /* loopback lan */
    "5: C0A8010B:0016 A9FE0001:1000 01 0\n";  /* link-local lan */
  build_wan_json(t, out, sizeof out);
  ASSERT(strstr(out, "\"public_count\":1"));
  ASSERT(strstr(out, "\"lan_count\":4"));
  ASSERT(strstr(out, "172.32.0.1:4096"));
}

TEST(wan_malformed_and_empty) {
  char out[128];
  const char *e = "{\"public_conns\":[],\"public_count\":0,\"lan_count\":0}";
  ASSERT(build_wan_json("", out, sizeof out) == (int)strlen(e));
  ASSERT_STR_EQ(out, e);
  build_wan_json(NULL, out, sizeof out); ASSERT_STR_EQ(out, e);
  build_wan_json("garbage\n:::\n1: ZZ:1 QQ:2 01\n", out, sizeof out); ASSERT_STR_EQ(out, e);
}

TEST(wan_overflow_returns_minus_one) {
  char out[8];
  ASSERT(build_wan_json(FX_TCP, out, sizeof out) == -1);
}
