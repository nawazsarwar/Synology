#include "test.h"
#include "parse_net.h"

static const char FX_NET[] =
#include "fixtures/proc_net_dev.txt"
;

TEST(net_emits_eth0_omits_lo) {
  char out[512];
  int n = build_net_json(FX_NET, out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"iface\":\"eth0\""));
  ASSERT(strstr(out, "\"rx_bytes\":9876543210"));
  ASSERT(strstr(out, "\"tx_bytes\":1234567890"));
  ASSERT(!strstr(out, "\"lo\""));
  ASSERT(out[0] == '[' && out[n - 1] == ']');
}

TEST(net_header_only_is_empty_array) {
  char out[64];
  const char *hdr = "Inter-|   Receive |  Transmit\n face |bytes packets\n";
  ASSERT(build_net_json(hdr, out, sizeof out) == 2);
  ASSERT_STR_EQ(out, "[]");
  ASSERT(build_net_json("", out, sizeof out) == 2);
  ASSERT(build_net_json(NULL, out, sizeof out) == 2);
}

TEST(net_overflow_returns_minus_one) {
  char out[8];
  ASSERT(build_net_json(FX_NET, out, sizeof out) == -1);
}
