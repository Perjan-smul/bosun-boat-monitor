// Minimale testopzet voor de pc-tests: TEST(naam) { CHECK(...); }
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

struct TestCase {
  const char *name;
  void (*fn)();
};
std::vector<TestCase> &testCases();
extern int checkFailures;
extern uint32_t fakeMillis;  // millis() in de tests (shim/Arduino.h)

struct TestReg {
  TestReg(const char *name, void (*fn)()) { testCases().push_back({name, fn}); }
};
#define TEST(name)                               \
  static void test_##name();                     \
  static TestReg reg_##name(#name, test_##name); \
  static void test_##name()

#define FAIL_AT(...)                                    \
  do {                                                  \
    checkFailures++;                                    \
    fprintf(stderr, "  %s:%d: ", __FILE__, __LINE__);   \
    fprintf(stderr, __VA_ARGS__);                       \
    fprintf(stderr, "\n");                              \
  } while (0)
#define CHECK(c) \
  do { if (!(c)) FAIL_AT("CHECK(%s)", #c); } while (0)
#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    long long va_ = (long long)(a), vb_ = (long long)(b);                                  \
    if (va_ != vb_) FAIL_AT("%s == %s: %lld != %lld", #a, #b, va_, vb_);                   \
  } while (0)
#define CHECK_NEAR(a, b, tol)                                                              \
  do {                                                                                     \
    double va_ = (a), vb_ = (b);                                                           \
    if (!(fabs(va_ - vb_) <= (tol))) FAIL_AT("%s ~ %s: %.6f != %.6f", #a, #b, va_, vb_);  \
  } while (0)
#define CHECK_STR(a, b)                                                                    \
  do {                                                                                     \
    const char *va_ = (a), *vb_ = (b);                                                     \
    if (!va_ || strcmp(va_, vb_)) FAIL_AT("%s: \"%s\" != \"%s\"", #a, va_ ? va_ : "(geen)", vb_); \
  } while (0)
