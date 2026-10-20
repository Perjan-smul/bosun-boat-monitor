#include "check.h"

std::vector<TestCase> &testCases() {
  static std::vector<TestCase> t;
  return t;
}
int checkFailures = 0;
uint32_t fakeMillis = 0;

int main() {
  int failed = 0;
  for (auto &t : testCases()) {
    int before = checkFailures;
    t.fn();
    bool ok = checkFailures == before;
    printf("%s %s\n", ok ? "ok  " : "FOUT", t.name);
    failed += !ok;
  }
  printf("%zu tests, %d fout\n", testCases().size(), failed);
  return failed ? 1 : 0;
}
