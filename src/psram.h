#pragma once
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

// Grote werkbuffers in PSRAM: het interne RAM is krap (Bluetooth, wifi, schermobjecten), PSRAM heeft MB's vrij.
// Eenmalig gereserveerd en nooit vrijgegeven; gevuld met nullen. Zonder PSRAM (of op de pc) gewoon calloc.
// Alleen voor eenvoudige structs zonder constructor.
template <typename T>
T *psram_alloc(size_t n) {
  void *p = nullptr;
#ifdef ESP_PLATFORM
  p = heap_caps_calloc(n, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  if (!p) p = calloc(n, sizeof(T));
  return (T *)p;
}
