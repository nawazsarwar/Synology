#include "test.h"
#include "json.h"

TEST(json_escapes_and_commas) {
  jb b; jb_init(&b);
  jb_raw(&b, "{"); jb_kstr(&b, "name", "a\"b"); jb_knum(&b, "n", 42); jb_raw(&b, "}");
  ASSERT_STR_EQ(b.buf, "{\"name\":\"a\\\"b\",\"n\":42}");
  jb_free(&b);
}

TEST(json_control_chars_and_real) {
  jb b; jb_init(&b);
  jb_raw(&b, "{"); jb_kstr(&b, "s", "x\n\\\x01"); jb_kreal(&b, "r", 1.5); jb_raw(&b, "}");
  ASSERT_STR_EQ(b.buf, "{\"s\":\"x\\n\\\\\\u0001\",\"r\":1.5}");
  jb_free(&b);
}
