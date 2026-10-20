#include "test.h"
#include "parse_system.h"

static const char FX_STAT[] =
#include "fixtures/proc_stat.txt"
;
static const char FX_MEMINFO[] =
#include "fixtures/meminfo.txt"
;
static const char FX_LOADAVG[] =
#include "fixtures/loadavg.txt"
;
static const char FX_UPTIME[] =
#include "fixtures/uptime.txt"
;

TEST(system_parses_fields) {
  char out[1024];
  int n = build_system_json(FX_STAT, FX_MEMINFO, "0.10 0.05 0.01 2/64 7562", "2913734.12 100.0", out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"mem_total\":61456"));
  ASSERT(strstr(out, "\"load1\":0.1"));
  ASSERT(strstr(out, "\"cpu_total\":"));
}

TEST(system_exact_values_from_fixtures) {
  char out[1024];
  int n = build_system_json(FX_STAT, FX_MEMINFO, FX_LOADAVG, FX_UPTIME, out, sizeof out);
  ASSERT(n > 0);
  ASSERT(strstr(out, "\"cpu_total\":22820"));
  ASSERT(strstr(out, "\"cpu_idle\":16250"));
  ASSERT(strstr(out, "\"mem_free\":5120"));
  ASSERT(strstr(out, "\"mem_cached\":30720"));
  ASSERT(strstr(out, "\"load5\":0.05"));
  ASSERT(strstr(out, "\"load15\":0.01"));
  ASSERT(strstr(out, "\"uptime_s\":2913734"));
}

TEST(system_handles_short_input) {
  char o[1024];
  ASSERT(build_system_json("cpu 1 2 3\n", "MemTotal: 10 kB\n", "0 0 0 1/1 1", "1.0 1.0", o, sizeof o) > 0);
}

TEST(system_overflow_returns_minus_one) {
  char o[16];
  ASSERT(build_system_json(FX_STAT, FX_MEMINFO, FX_LOADAVG, FX_UPTIME, o, sizeof o) == -1);
}
