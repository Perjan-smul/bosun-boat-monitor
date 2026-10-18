#include "log.h"
#include "ble.h"
#include <NimBLEDevice.h>
#include "quarantine.h"
#include "store.h"

namespace {

const NimBLEUUID UUID_181A((uint16_t)0x181A);
const NimBLEUUID UUID_BTHOME((uint16_t)0xFCD2);

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *dev) override {
    std::string mac = dev->getAddress().toString();
    if (quarantine_ble_blocked(mac.c_str())) return;  // uitgeschakeld na crashes
    quarantine_enter_ble(mac.c_str());
    std::string mfg = dev->getManufacturerData();
    std::string svc181a, bthome;
    for (size_t i = 0; i < dev->getServiceDataCount(); i++) {
      NimBLEUUID uuid = dev->getServiceDataUUID(i);
      if (uuid == UUID_181A) svc181a = dev->getServiceData(i);
      else if (uuid == UUID_BTHOME) bthome = dev->getServiceData(i);
    }

    AdvData adv;
    adv.mopekaSvc = dev->isAdvertisingService(NimBLEUUID((uint16_t)0xFEE5));
    adv.thermoBeaconSvc = dev->isAdvertisingService(NimBLEUUID((uint16_t)0xFFF0));
    if (!mfg.empty()) { adv.mfg = (const uint8_t *)mfg.data(); adv.mfgLen = mfg.size(); }
    if (!svc181a.empty()) { adv.svc181a = (const uint8_t *)svc181a.data(); adv.svc181aLen = svc181a.size(); }
    if (!bthome.empty()) { adv.bthome = (const uint8_t *)bthome.data(); adv.bthomeLen = bthome.size(); }

    std::string name = dev->haveName() ? dev->getName() : "";
    store_on_advert(mac.c_str(), name.c_str(), dev->getRSSI(), adv);
    quarantine_leave_ble();
  }
};

ScanCallbacks callbacks;

void onScanEnd(NimBLEScanResults) {
  // Zou niet moeten gebeuren (duur 0 = oneindig), maar herstart voor de zekerheid
  NimBLEDevice::getScan()->start(0, onScanEnd, false);
}

}  // namespace

void ble_begin() {
  NimBLEDevice::init("Bosun");
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(&callbacks, true);  // ook herhaalde pakketten: sensoren sturen nieuwe waarden
  scan->setActiveScan(true);                              // voor apparaatnamen in scan responses
  scan->setDuplicateFilter(false);
  scan->setMaxResults(0);                                 // niets bufferen, alles gaat via de callback
  scan->setInterval(100);
  scan->setWindow(90);
  scan->start(0, onScanEnd, false);
  log_line("[ble] scannen gestart");
}
