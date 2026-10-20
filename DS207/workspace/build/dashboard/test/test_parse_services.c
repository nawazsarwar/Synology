#include "test.h"
#include "parse_services.h"

TEST(services_up_with_version_and_down_null) {
  svc_probe p[2] = { { "smbd", 1, "Version 3.6.25" }, { "afpd", 0, NULL } };
  char out[256];
  int n = build_services_json(p, 2, out, sizeof out);
  ASSERT_STR_EQ(out,
    "[{\"name\":\"smbd\",\"up\":true,\"version\":\"Version 3.6.25\"},"
    "{\"name\":\"afpd\",\"up\":false,\"version\":null}]");
  ASSERT(n == (int)strlen(out));
}

TEST(services_nonzero_is_true_and_escaping) {
  svc_probe p[1] = { { "x\"y", 7, "a\\b" } };
  char out[128];
  ASSERT(build_services_json(p, 1, out, sizeof out) > 0);
  ASSERT_STR_EQ(out, "[{\"name\":\"x\\\"y\",\"up\":true,\"version\":\"a\\\\b\"}]");
}

TEST(services_empty_is_empty_array) {
  char out[16];
  ASSERT(build_services_json(NULL, 0, out, sizeof out) == 2);
  ASSERT_STR_EQ(out, "[]");
}

TEST(services_overflow_and_bad_args) {
  svc_probe p[1] = { { "smbd", 1, "1" } };
  char out[8];
  ASSERT(build_services_json(p, 1, out, sizeof out) == -1);
  ASSERT(build_services_json(p, 1, NULL, 0) == -1);
  ASSERT(build_services_json(NULL, 1, out, sizeof out) == -1);
}
