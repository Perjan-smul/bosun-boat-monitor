#include "buzzer.h"
#include "wsio.h"

// Zoemer op het board, via de hulpchip
namespace {
bool present = false;
}  // namespace

void buzzer_begin() {
  present = wsio_type() != WsIo::None;
  if (present) buzzer_set(false);
}

bool buzzer_present() { return present; }

void buzzer_set(bool on) { wsio_buzzer(on); }
