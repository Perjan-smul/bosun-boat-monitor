#include "ota_guard.h"

BootAction otaOnBoot(OtaRecord &r) {
  if (!r.pending) return BootAction::Normal;
  r.tries++;
  if (r.tries > OTA_MAX_TRIES) {
    r.pending = false;
    r.tries = 0;
    return BootAction::Revert;
  }
  return BootAction::WaitHealthy;
}

void otaStage(OtaRecord &r) {
  r.pending = true;
  r.tries = 0;
}

void otaMarkHealthy(OtaRecord &r) {
  r.pending = false;
  r.tries = 0;
}
