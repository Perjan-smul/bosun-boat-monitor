// Vervangt Arduino.h voor de pc-tests: alleen wat de geteste code gebruikt.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

using std::max;
using std::min;

#if !(defined(__GLIBC__) && (__GLIBC__ > 2 || __GLIBC_MINOR__ >= 38))  // nieuwere glibc heeft hem al
inline size_t strlcpy(char *dst, const char *src, size_t size) {
  size_t n = strlen(src);
  if (size) {
    size_t k = n < size - 1 ? n : size - 1;
    memcpy(dst, src, k);
    dst[k] = 0;
  }
  return n;
}
#endif
