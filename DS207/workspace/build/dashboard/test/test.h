/* Minimal C99 test harness. One test file = one translation unit; test.h defines main(). */
#ifndef TEST_H
#define TEST_H
#include <stdio.h>
#include <string.h>

#define TEST_MAX 256
typedef void (*test_fn)(void);
static struct { const char *name; test_fn fn; } test_reg[TEST_MAX];
static int test_n = 0, test_failed = 0;

static void test_register(const char *name, test_fn fn) {
  if (test_n < TEST_MAX) { test_reg[test_n].name = name; test_reg[test_n].fn = fn; test_n++; }
}

#define TEST(name) \
  static void name(void); \
  static void __attribute__((constructor)) name##_reg(void) { test_register(#name, name); } \
  static void name(void)

#define ASSERT(cond) do { if (!(cond)) { \
  printf("    ASSERT failed %s:%d: %s\n", __FILE__, __LINE__, #cond); test_failed++; } } while (0)

#define ASSERT_STR_EQ(a, b) do { const char *_a = (a), *_b = (b); \
  if (!_a || !_b || strcmp(_a, _b) != 0) { \
    printf("    ASSERT_STR_EQ failed %s:%d:\n      got:      %s\n      expected: %s\n", \
           __FILE__, __LINE__, _a ? _a : "(null)", _b ? _b : "(null)"); test_failed++; } } while (0)

int main(void) {
  int i, before;
  for (i = 0; i < test_n; i++) {
    before = test_failed;
    printf("[ RUN  ] %s\n", test_reg[i].name);
    test_reg[i].fn();
    printf("[ %s ] %s\n", test_failed == before ? " OK " : "FAIL", test_reg[i].name);
  }
  printf("%d test(s), %d assertion failure(s)\n", test_n, test_failed);
  return test_failed ? 1 : 0;
}
#endif
