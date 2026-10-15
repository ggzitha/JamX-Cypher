/*
 * CYPHER BOX - 2.4GHz Multi-Protocol Jammer
 * Target: ESP32-C3
 * Radios: 2x NRF24L01+ (PA+LNA)
 * Display: SSD1306 128x64 I2C
 *
 * FOR EDUCATIONAL / LAB TESTING ONLY
 *
 * CHANNEL MAPPING NOTES (fixed):
 *   NRF24 channel N = (frequency_MHz - 2400)
 *   WiFi  ch 1  (2412 MHz) = NRF 12
 *   WiFi  ch 6  (2437 MHz) = NRF 37
 *   WiFi  ch 11 (2462 MHz) = NRF 62
 *   BLE adv 37  (2402 MHz) = NRF  2
 *   BLE adv 38  (2426 MHz) = NRF 26
 *   BLE adv 39  (2480 MHz) = NRF 80
 *   BLE data channels = even NRF channels 2..80
 */

#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_bt.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "FS.h"
#include "SPIFFS.h"
#include "RF24.h"
#include <SPI.h>

// ===================== PIN DEFINITIONS =====================
#define SDA_PIN 8
#define SCL_PIN 9
#define SCK_PIN 4
#define MISO_PIN 5
#define MOSI_PIN 6

#define RADIO1_CE 20
#define RADIO1_CS 21
#define RADIO2_CE 7
#define RADIO2_CS 10

#define LED_PIN 99
#define UP_BUTTON_PIN 3
#define DOWN_BUTTON_PIN 0
#define SELECT_BUTTON_PIN 1

// ===================== DISPLAY =====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SSD1306_I2C_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
U8G2_FOR_ADAFRUIT_GFX u8g2_for_adafruit_gfx;

// ===================== NRF24 =====================
SPIClass spiBus(FSPI);
RF24 radio(RADIO1_CE, RADIO1_CS, 16000000);
RF24 radio2(RADIO2_CE, RADIO2_CS, 16000000);

// ===================== PROTOCOL DEFINITIONS =====================
enum ProtocolType {
  PROTO_BT_CLASSIC = 0,
  PROTO_BLE,
  PROTO_WIFI,
  PROTO_DRONE,
  PROTO_MULTI,
  PROTO_COUNT
};

enum TargetMode {
  TARGET_ALL = 0,
  TARGET_AUDIO,
  TARGET_HID,
  TARGET_BEACON,
  TARGET_COUNT
};

// ===================== PRESET LIBRARY =====================
struct JamPreset {
  const char* name;
  const uint8_t* channels;
  uint8_t count;
  uint8_t weight;
};

// --- Preset channel arrays (NRF24 channel numbers) ---

// Full spectrum (all 126 NRF channels 0-125)
const uint8_t CH_FULL[] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
  26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,
  50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,
  74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,
  98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,
  116,117,118,119,120,121,122,123,124,125
};

// --- FIXED: WiFi 2.4GHz channel 1-13 mapped to NRF channels ---
// WiFi ch 1=2412->NRF12, 2=2417->17, 3=2422->22, 4=2427->27, 5=2432->32,
// 6=2437->37, 7=2442->42, 8=2447->47, 9=2452->52, 10=2457->57,
// 11=2462->62, 12=2467->67, 13=2472->72
const uint8_t CH_WIFI[] = {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72};

// WiFi non-overlapping priority (ch 1, 6, 11) -> NRF 12, 37, 62
const uint8_t CH_WIFI_PRI[] = {12, 37, 62};

// --- FIXED: BLE all 40 data channels (even NRF 2..80) ---
// BLE data ch 0=2404->NRF4, ch 10=2424->24, ch 11=2428->28, ..., ch 36=2478->78
// BLE adv ch 37=2402->NRF2, ch 38=2426->26, ch 39=2480->80
const uint8_t CH_BLE_ALL[] = {
  2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,
  42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80
};

// --- FIXED: BLE advertising channels (37,38,39) -> NRF 2, 26, 80 ---
const uint8_t CH_BLE_ADV[] = {2, 26, 80};

// BLE advertising + close neighbors in NRF space
const uint8_t CH_BLE_ADV_PRI[] = {2, 24, 26, 28, 78, 80};

// Bluetooth Classic all 79 hop channels (2402-2480 MHz = NRF 2..80)
const uint8_t CH_BT_CLASSIC[] = {
  2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
  26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
  49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,
  72,73,74,75,76,77,78,79,80
};

// BT Classic inquiry/paging channels
const uint8_t CH_BT_INQUIRY[] = {2, 34, 50, 66};

// Drone FHSS sequence
const uint8_t CH_DRONE_FHSS[] = {0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95};

// Drone full range 0-124
const uint8_t CH_DRONE_FULL[] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,
  26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
  49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,
  72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,
  95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,
  114,115,116,117,118,119,120,121,122,123,124
};

const uint8_t CH_USB_DONGLE[] = {40, 50, 60, 70, 80, 90};
const uint8_t CH_FPV_VIDEO[]  = {70, 75, 80, 85, 90, 95};
const uint8_t CH_RC_CONTROL[] = {4, 8, 12, 16};
const uint8_t CH_ZIGBEE[]     = {15, 20, 25, 30};

// --- Standalone constants used by fallback pickers ---
const uint8_t BLE_ADV_CH[3]    = {2, 26, 80};    // FIXED
const uint8_t WIFI_PRIORITY[3] = {12, 37, 62};   // FIXED

// --- Preset table ---
const JamPreset PRESETS[] = {
  { "Full Spectrum",  CH_FULL,         sizeof(CH_FULL),         10 },
  { "WiFi 2.4GHz",    CH_WIFI,         sizeof(CH_WIFI),         15 },
  { "WiFi Priority",  CH_WIFI_PRI,     sizeof(CH_WIFI_PRI),     20 },
  { "BLE All Data",   CH_BLE_ALL,      sizeof(CH_BLE_ALL),      15 },
  { "BLE Adv Pri",    CH_BLE_ADV,      sizeof(CH_BLE_ADV),      25 },
  { "BLE Adv + Nb",   CH_BLE_ADV_PRI,  sizeof(CH_BLE_ADV_PRI),  20 },
  { "BT Classic",     CH_BT_CLASSIC,   sizeof(CH_BT_CLASSIC),   15 },
  { "BT Inquiry",     CH_BT_INQUIRY,   sizeof(CH_BT_INQUIRY),   20 },
  { "Drone FHSS",     CH_DRONE_FHSS,   sizeof(CH_DRONE_FHSS),   15 },
  { "Drone Full",     CH_DRONE_FULL,   sizeof(CH_DRONE_FULL),   10 },
  { "USB Dongle",     CH_USB_DONGLE,   sizeof(CH_USB_DONGLE),   15 },
  { "FPV Video",      CH_FPV_VIDEO,    sizeof(CH_FPV_VIDEO),    10 },
  { "RC Control",     CH_RC_CONTROL,   sizeof(CH_RC_CONTROL),   10 },
  { "Zigbee",         CH_ZIGBEE,       sizeof(CH_ZIGBEE),       10 },
};

const uint8_t NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

// ===================== SETTINGS STRUCT =====================
#define SETTINGS_FILE "/settings.bin"
#define SETTINGS_MAGIC 0xCB03   // bumped to force defaults on first boot

struct DeviceSettings {
  uint16_t magic;
  uint8_t  paLevel;
  uint8_t  dataRate;
  uint16_t hopDelayUs;
  uint8_t  activeRadios;
  uint8_t  protocol;
  uint8_t  targetMode;
  bool     autoChannel;
  bool     usePresets;
  uint8_t  presetIndex;
  uint8_t  scanThreshold;
  uint8_t  fixedCh1;
  uint8_t  fixedCh2;
};

const DeviceSettings DEFAULT_SETTINGS = {
  SETTINGS_MAGIC,
  3, 2, 50, 0,
  PROTO_BT_CLASSIC,
  TARGET_ALL,
  false, false, 0,
  40,
  0, 0
};

DeviceSettings config = DEFAULT_SETTINGS;

// ===================== SETTINGS MENU =====================
enum SettingsItem {
  SET_PA_LEVEL,
  SET_DATA_RATE,
  SET_HOP_DELAY,
  SET_ACTIVE_RADIO,
  SET_PROTOCOL,
  SET_TARGET,
  SET_AUTO_CH,
  SET_USE_PRESETS,
  SET_PRESET_SEL,
  SET_SCAN_THRESH,
  SET_FIXED_CH1,
  SET_FIXED_CH2,
  SET_RESET,
  SET_SAVE,
  SET_BACK,
  NUM_SETTINGS_ITEMS
};

int selectedSettingsItem = 0;
int firstVisibleSettingsItem = 0;

const int NUM_HELP_PAGES = 6;
int currentHelpPage = 0;

// ===================== APP STATE =====================
enum AppState {
  STATE_MENU,
  STATE_BT_JAM,
  STATE_DRONE_JAM,
  STATE_WIFI_JAM,
  STATE_MULTI_JAM,
  STATE_TEST_RADIOS,
  STATE_SPECTRUM_SCAN,
  STATE_SETTINGS,
  STATE_HELP
};
AppState currentState = STATE_MENU;

// ===================== MENU =====================
enum MenuItem {
  BT_JAM,
  DRONE_JAM,
  WIFI_JAM,
  MULTI_JAM,
  SPECTRUM_SCAN,
  TEST_RADIOS,
  SETTINGS,
  HELP,
  NUM_MENU_ITEMS
};

MenuItem selectedMenuItem = BT_JAM;
int firstVisibleMenuItem = 0;

// ===================== HELPERS =====================
void nonBlockingDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) yield();
}

void recoverI2CBus() {
  Wire.end();
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delayMicroseconds(50);
  if (digitalRead(SDA_PIN) == LOW) {
    pinMode(SCL_PIN, OUTPUT);
    for (int i = 0; i < 16; i++) {
      digitalWrite(SCL_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(SCL_PIN, LOW);
      delayMicroseconds(10);
    }
    pinMode(SDA_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);
    delayMicroseconds(10);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SDA_PIN, HIGH);
    delayMicroseconds(10);
  }
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
}

void recoverDisplay() {
  recoverI2CBus();
  display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);
  u8g2_for_adafruit_gfx.begin(display);
}

void initDisplay() {
  recoverDisplay();
  display.display();
  delay(500);
  display.clearDisplay();
}

// ===================== SPIFFS =====================
bool saveSettings() {
  File file = SPIFFS.open(SETTINGS_FILE, "w");
  if (!file) return false;
  size_t written = file.write((const uint8_t *)&config, sizeof(DeviceSettings));
  file.close();
  return (written == sizeof(DeviceSettings));
}

void loadSettings() {
  if (SPIFFS.exists(SETTINGS_FILE)) {
    File file = SPIFFS.open(SETTINGS_FILE, "r");
    if (file && file.size() == sizeof(DeviceSettings)) {
      file.read((uint8_t *)&config, sizeof(DeviceSettings));
      file.close();
      if (config.magic != SETTINGS_MAGIC) {
        config = DEFAULT_SETTINGS;
        return;
      }
      if (config.paLevel > 3) config.paLevel = 3;
      if (config.dataRate > 2) config.dataRate = 2;
      if (config.activeRadios > 2) config.activeRadios = 0;
      if (config.protocol >= PROTO_COUNT) config.protocol = PROTO_BT_CLASSIC;
      if (config.targetMode >= TARGET_COUNT) config.targetMode = TARGET_ALL;
      if (config.presetIndex >= NUM_PRESETS) config.presetIndex = 0;
      if (config.fixedCh1 > 125) config.fixedCh1 = 0;
      if (config.fixedCh2 > 125) config.fixedCh2 = 0;
      if (config.scanThreshold > 100) config.scanThreshold = 40;
      return;
    }
    if (file) file.close();
  }
  config = DEFAULT_SETTINGS;
}

void resetDefaults() {
  config = DEFAULT_SETTINGS;
  saveSettings();
}

// ===================== NRF24 SETUP =====================
void applyRadioConfig(RF24 &r) {
  const rf24_pa_dbm_e paMap[] = { RF24_PA_MIN, RF24_PA_LOW, RF24_PA_HIGH, RF24_PA_MAX };
  const rf24_datarate_e drMap[] = { RF24_250KBPS, RF24_1MBPS, RF24_2MBPS };
  r.setAutoAck(false);
  r.stopListening();
  r.setRetries(0, 0);
  r.setPALevel(paMap[config.paLevel % 4], true);
  r.setDataRate(drMap[config.dataRate % 3]);
  r.setCRCLength(RF24_CRC_DISABLED);
}

void initRadios() {
  delay(50);
  spiBus.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Init Radios...");

  rf24_pa_dbm_e currentPA = (rf24_pa_dbm_e)config.paLevel;

  if (config.activeRadios == 0 || config.activeRadios == 1) {
    if (radio.begin(&spiBus)) {
      display.println("Radio 1: OK");
      applyRadioConfig(radio);
      radio.startConstCarrier(currentPA, 45);
    } else {
      display.println("Radio 1: FAIL");
    }
  } else display.println("Radio 1: OFF");

  if (config.activeRadios == 0 || config.activeRadios == 2) {
    if (radio2.begin(&spiBus)) {
      display.println("Radio 2: OK");
      applyRadioConfig(radio2);
      radio2.startConstCarrier(currentPA, 45);
    } else {
      display.println("Radio 2: FAIL");
    }
  } else display.println("Radio 2: OFF");

  display.display();
}

void testRadios() {
  spiBus.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (radio.begin(&spiBus)) { display.println("Radio 1: OK"); radio.printPrettyDetails(); }
  else display.println("Radio 1: FAIL");
  if (radio2.begin(&spiBus)) { display.println("Radio 2: OK"); radio2.printPrettyDetails(); }
  else display.println("Radio 2: FAIL");

  display.println("");
  display.println("[SEL] Exit");
  display.display();
}

// ===================== RSSI SCANNER (RPD) =====================
const int SCAN_SAMPLES = 30;
uint8_t scanResults[128] = { 0 };

bool testRPD(RF24 &r) {
  r.startListening();
  delayMicroseconds(130);
  bool result = r.testRPD();
  r.stopListening();
  return result;
}

void scanChannels(RF24 &r, uint8_t numChannels, uint8_t *results) {
  for (uint8_t ch = 0; ch < numChannels; ch++) {
    r.setChannel(ch);
    uint8_t hits = 0;
    for (int s = 0; s < SCAN_SAMPLES; s++) {
      if (testRPD(r)) hits++;
      delayMicroseconds(50);
    }
    results[ch] = (hits * 100) / SCAN_SAMPLES;
  }
}

void scanChannelsDual() {
  const uint8_t noiseAddress[][2] = {
    { 0x55, 0x55 }, { 0xAA, 0xAA }, { 0xA0, 0xAA },
    { 0xAB, 0xAA }, { 0xAC, 0xAA }, { 0xAD, 0xAA }
  };

  if (config.activeRadios == 0 || config.activeRadios == 1) {
    radio.begin(&spiBus);
    radio.setAutoAck(false);
    radio.setDataRate(RF24_1MBPS);
    radio.setPALevel(RF24_PA_MIN);
    radio.setAddressWidth(2);
    radio.openReadingPipe(0, noiseAddress[0]);
    radio.disableCRC();
    radio.setChannel(0);
  }
  if (config.activeRadios == 0 || config.activeRadios == 2) {
    radio2.begin(&spiBus);
    radio2.setAutoAck(false);
    radio2.setDataRate(RF24_1MBPS);
    radio2.setPALevel(RF24_PA_MIN);
    radio2.setAddressWidth(2);
    radio2.openReadingPipe(0, noiseAddress[0]);
    radio2.disableCRC();
    radio2.setChannel(63);
  }

  if (config.activeRadios == 0 || config.activeRadios == 1) scanChannels(radio, 63, scanResults);
  if (config.activeRadios == 0 || config.activeRadios == 2) scanChannels(radio2, 63, scanResults + 63);
}

void drawSpectrum() {
  recoverI2CBus();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 2);
  display.setTextSize(1);
  display.print("SPECTRUM SCAN");

  for (int i = 0; i < 63; i++) {
    uint8_t val = (scanResults[i * 2] + scanResults[i * 2 + 1]) / 2;
    if (val > 100) val = 100;
    int barH = map(val, 0, 100, 0, 38);
    if (barH > 0) display.fillRect(i * 2 + 1, 50 - barH, 2, barH, SSD1306_WHITE);
  }

  display.setTextSize(1);
  display.setCursor(0, 52);   display.print("0");
  display.setCursor(56, 52);  display.print("63");
  display.setCursor(112, 52); display.print("125");
  display.display();
}

// ===================== JAM CORE =====================

void presetPickDual(uint8_t presetIdx, uint8_t &c1, uint8_t &c2) {
  if (presetIdx >= NUM_PRESETS) presetIdx = 0;
  const JamPreset &p = PRESETS[presetIdx];
  if (p.count == 1) { c1 = c2 = p.channels[0]; return; }
  c1 = p.channels[random(p.count)];
  do { c2 = p.channels[random(p.count)]; } while (c2 == c1 && p.count > 1);
}

// --- FIXED: autoPickChannel now respects config.scanThreshold ---
// Only channels whose activity is >= threshold are considered.
uint8_t autoPickChannel() {
  uint32_t total = 0;
  for (int i = 0; i < 126; i++) {
    if (scanResults[i] >= config.scanThreshold) total += scanResults[i];
  }
  if (total == 0) return random(126);

  uint32_t pick = random(total);
  uint32_t accum = 0;
  for (int i = 0; i < 126; i++) {
    if (scanResults[i] < config.scanThreshold) continue;
    accum += scanResults[i];
    if (pick < accum) return i;
  }
  return random(126);
}

void autoPickDual(uint8_t &c1, uint8_t &c2) {
  c1 = autoPickChannel();
  c2 = autoPickChannel();
  if (c1 == c2 && random(100) < 50) c2 = (c1 + random(1, 10)) % 126;
}

void setBothChannels(uint8_t ch1, uint8_t ch2) {
  if (config.activeRadios == 0 || config.activeRadios == 1) radio.setChannel(ch1);
  if (config.activeRadios == 0 || config.activeRadios == 2) radio2.setChannel(ch2);
}

// --- FIXED: BLE target picker returns real BLE NRF channels (even 2..80) ---
uint8_t pickBleTargetChannel() {
  switch (config.targetMode) {
    case TARGET_AUDIO:
    case TARGET_HID:
    case TARGET_BEACON:
      if (random(100) < 70) return BLE_ADV_CH[random(3)];  // NRF 2, 26, 80
      return 2 + random(40) * 2;                            // even 2..80
    default:
      return 2 + random(40) * 2;
  }
}

// --- FIXED: BT classic real range is NRF 2..80 ---
uint8_t pickBtClassicChannel() {
  return 2 + random(79);
}

// --- FIXED: WiFi picker returns real NRF channels ---
uint8_t pickWifiChannel() {
  if (random(100) < 60) return WIFI_PRIORITY[random(3)];  // NRF 12, 37, 62
  return 12 + random(13) * 5;                              // one of 13 WiFi channels
}

uint8_t pickDroneChannel() {
  if (random(100) < 75) return random(0, 84);
  return random(125);
}

// ===================== UNIFIED JAM STEP =====================
void jamStep() {
  uint8_t c1, c2;

  if (config.autoChannel) {
    static uint32_t hopCount = 0;
    if (hopCount % 500 == 0) scanChannelsDual();
    hopCount++;
    autoPickDual(c1, c2);
  } else if (config.usePresets) {
    presetPickDual(config.presetIndex, c1, c2);
  } else if (config.fixedCh1 != 0 || config.fixedCh2 != 0) {
    c1 = config.fixedCh1;
    c2 = config.fixedCh2;
  } else {
    switch (config.protocol) {
      case PROTO_BT_CLASSIC:
        c1 = pickBtClassicChannel(); c2 = pickBtClassicChannel(); break;
      case PROTO_BLE:
        c1 = pickBleTargetChannel(); c2 = pickBleTargetChannel(); break;
      case PROTO_WIFI:
        c1 = pickWifiChannel();      c2 = pickWifiChannel();      break;
      case PROTO_DRONE:
        c1 = pickDroneChannel();     c2 = pickDroneChannel();     break;
      default:
        c1 = random(126);            c2 = random(126);            break;
    }
  }

  setBothChannels(c1, c2);
  if (config.hopDelayUs > 0) delayMicroseconds(config.hopDelayUs);
}

// ===================== DISPLAY UI =====================
void drawBorder() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
}

void displayInfo(String title, String info1 = "", String info2 = "", String info3 = "") {
  display.clearDisplay();
  drawBorder();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 4);
  display.println(title);
  display.drawLine(0, 14, SCREEN_WIDTH, 14, SSD1306_WHITE);
  display.setCursor(4, 18); display.println(info1);
  display.setCursor(4, 28); display.println(info2);
  display.setCursor(4, 38); display.println(info3);
  display.display();
}

bool isButtonPressed(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    delay(80);
    if (digitalRead(pin) == LOW) {
      if (LED_PIN < 40) digitalWrite(LED_PIN, HIGH);
      return true;
    }
  }
  return false;
}

void drawMenu() {
  recoverI2CBus();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.fillRect(0, 0, SCREEN_WIDTH, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(5, 4);
  display.println("Home");

  const char *menuLabels[NUM_MENU_ITEMS] = {
    "BT/BLE JamX", "Drone JamX", "Wifi JamX",
    "MultiCH JamX", "Spectrum Scan", "Test Radios", "Settings", "Help"
  };

  display.setTextColor(SSD1306_WHITE);
  for (int i = 0; i < 2; i++) {
    int menuIndex = (firstVisibleMenuItem + i) % NUM_MENU_ITEMS;
    int16_t y = 20 + (i * 20);
    if (selectedMenuItem == menuIndex) {
      display.fillRect(0, y - 2, SCREEN_WIDTH, 15, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else display.setTextColor(SSD1306_WHITE);
    display.setCursor(5, y);
    display.println(menuLabels[menuIndex]);
  }
  display.display();
}

void showPopup(const char *line1, const char *line2 = "") {
  display.clearDisplay();
  display.drawRect(4, 8, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 16, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 22); display.println(line1);
  if (line2 && line2[0]) { display.setCursor(10, 36); display.println(line2); }
  display.display();
}

// ===================== SETTINGS UI =====================
void getSettingsItemText(int item, char *buf, size_t bufSize) {
  switch (item) {
    case SET_PA_LEVEL: {
      const char *n[] = { "MIN (-18)", "LOW (-12)", "HIGH(-6)", "MAX (0)" };
      snprintf(buf, bufSize, "PA: %s", n[config.paLevel % 4]); break;
    }
    case SET_DATA_RATE: {
      const char *n[] = { "250K", "1M", "2M" };
      snprintf(buf, bufSize, "Rate: %s", n[config.dataRate % 3]); break;
    }
    case SET_HOP_DELAY: snprintf(buf, bufSize, "Hop: %u us", config.hopDelayUs); break;
    case SET_ACTIVE_RADIO: {
      const char *n[] = { "Dual", "Radio1", "Radio2" };
      snprintf(buf, bufSize, "Rad: %s", n[config.activeRadios % 3]); break;
    }
    case SET_PROTOCOL: {
      const char *n[] = { "BT Classic", "BLE", "WiFi", "Drone", "Multi" };
      snprintf(buf, bufSize, "Proto: %s", n[config.protocol % PROTO_COUNT]); break;
    }
    case SET_TARGET: {
      const char *n[] = { "All", "Audio", "HID", "Beacon" };
      snprintf(buf, bufSize, "Target: %s", n[config.targetMode % TARGET_COUNT]); break;
    }
    case SET_AUTO_CH:       snprintf(buf, bufSize, "Auto: %s", config.autoChannel ? "ON" : "OFF"); break;
    case SET_USE_PRESETS:   snprintf(buf, bufSize, "Preset: %s", config.usePresets ? "ON" : "OFF"); break;
    case SET_PRESET_SEL:    snprintf(buf, bufSize, "> %s", PRESETS[config.presetIndex].name); break;
    case SET_SCAN_THRESH:   snprintf(buf, bufSize, "Thr: %u%%", config.scanThreshold); break;
    case SET_FIXED_CH1:     snprintf(buf, bufSize, "R1 Ch: %u", config.fixedCh1); break;
    case SET_FIXED_CH2:     snprintf(buf, bufSize, "R2 Ch: %u", config.fixedCh2); break;
    case SET_RESET:         snprintf(buf, bufSize, "< Reset Defaults >"); break;
    case SET_SAVE:          snprintf(buf, bufSize, "[ Save & Exit ]"); break;
    case SET_BACK:          snprintf(buf, bufSize, "[ Back / Discard ]"); break;
    default: buf[0] = '\0'; break;
  }
}

void drawSettingsMenu() {
  recoverI2CBus();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.fillRect(0, 0, SCREEN_WIDTH, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(4, 3);
  display.print("SETTINGS");

  char pageBuf[10];
  snprintf(pageBuf, sizeof(pageBuf), "%d/%d", selectedSettingsItem + 1, NUM_SETTINGS_ITEMS);
  display.setCursor(SCREEN_WIDTH - 28, 3);
  display.print(pageBuf);

  for (int i = 0; i < 3; i++) {
    int itemIndex = firstVisibleSettingsItem + i;
    if (itemIndex >= NUM_SETTINGS_ITEMS) break;
    int16_t y = 16 + (i * 16);
    char itemBuf[32];
    getSettingsItemText(itemIndex, itemBuf, sizeof(itemBuf));
    if (selectedSettingsItem == itemIndex) {
      display.fillRect(0, y, SCREEN_WIDTH, 14, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, y + 3);
    display.print(itemBuf);
  }
  display.display();
}

void handleSettingsSelection() {
  static bool btnPressed = false;

  if (!btnPressed) {
    if (isButtonPressed(UP_BUTTON_PIN)) {
      selectedSettingsItem = (selectedSettingsItem == 0) ? (NUM_SETTINGS_ITEMS - 1) : (selectedSettingsItem - 1);
      if (selectedSettingsItem < firstVisibleSettingsItem) firstVisibleSettingsItem = selectedSettingsItem;
      else if (selectedSettingsItem == NUM_SETTINGS_ITEMS - 1) firstVisibleSettingsItem = max(0, NUM_SETTINGS_ITEMS - 3);
      drawSettingsMenu();
      btnPressed = true;
    } else if (isButtonPressed(DOWN_BUTTON_PIN)) {
      selectedSettingsItem = (selectedSettingsItem + 1) % NUM_SETTINGS_ITEMS;
      if (selectedSettingsItem >= firstVisibleSettingsItem + 3) firstVisibleSettingsItem = selectedSettingsItem - 2;
      else if (selectedSettingsItem == 0) firstVisibleSettingsItem = 0;
      drawSettingsMenu();
      btnPressed = true;
    } else if (isButtonPressed(SELECT_BUTTON_PIN)) {
      btnPressed = true;
      switch (selectedSettingsItem) {
        case SET_PA_LEVEL:     config.paLevel = (config.paLevel + 1) % 4; drawSettingsMenu(); break;
        case SET_DATA_RATE:    config.dataRate = (config.dataRate + 1) % 3; drawSettingsMenu(); break;
        case SET_HOP_DELAY:
          if (config.hopDelayUs == 0) config.hopDelayUs = 20;
          else if (config.hopDelayUs == 20) config.hopDelayUs = 50;
          else if (config.hopDelayUs == 50) config.hopDelayUs = 100;
          else config.hopDelayUs = 0;
          drawSettingsMenu(); break;
        case SET_ACTIVE_RADIO: config.activeRadios = (config.activeRadios + 1) % 3; drawSettingsMenu(); break;
        case SET_PROTOCOL:     config.protocol = (config.protocol + 1) % PROTO_COUNT; drawSettingsMenu(); break;
        case SET_TARGET:       config.targetMode = (config.targetMode + 1) % TARGET_COUNT; drawSettingsMenu(); break;
        case SET_AUTO_CH:
          config.autoChannel = !config.autoChannel;
          if (config.autoChannel) config.usePresets = false;
          drawSettingsMenu(); break;
        case SET_USE_PRESETS:
          config.usePresets = !config.usePresets;
          if (config.usePresets) config.autoChannel = false;
          drawSettingsMenu(); break;
        case SET_PRESET_SEL:   config.presetIndex = (config.presetIndex + 1) % NUM_PRESETS; drawSettingsMenu(); break;
        case SET_SCAN_THRESH:  config.scanThreshold = (config.scanThreshold + 10) % 110; drawSettingsMenu(); break;
        case SET_FIXED_CH1:    config.fixedCh1 = (config.fixedCh1 + 5) % 126; drawSettingsMenu(); break;
        case SET_FIXED_CH2:    config.fixedCh2 = (config.fixedCh2 + 5) % 126; drawSettingsMenu(); break;
        case SET_RESET:
          resetDefaults();
          showPopup("DEFAULTS RESTORED", "All settings reset");
          delay(1200);
          drawSettingsMenu(); break;
        case SET_SAVE:
          saveSettings();
          showPopup("SETTINGS SAVED!", "Stored to SPIFFS");
          delay(1200);
          currentState = STATE_MENU;
          drawMenu(); break;
        case SET_BACK:
          loadSettings();
          currentState = STATE_MENU;
          drawMenu(); break;
      }
    }
  } else {
    if (!isButtonPressed(UP_BUTTON_PIN) && !isButtonPressed(DOWN_BUTTON_PIN) && !isButtonPressed(SELECT_BUTTON_PIN)) {
      btnPressed = false;
      if (LED_PIN < 40) digitalWrite(LED_PIN, LOW);
    }
  }
}

// ===================== HELP =====================
void drawHelpScreen(int page) {
  recoverI2CBus();
  display.clearDisplay();
  display.fillRect(0, 0, SCREEN_WIDTH, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(4, 3);
  display.print("HELP & GUIDE");

  char pageBuf[10];
  snprintf(pageBuf, sizeof(pageBuf), "%d/%d", page + 1, NUM_HELP_PAGES);
  display.setCursor(SCREEN_WIDTH - 28, 3);
  display.print(pageBuf);

  display.setTextColor(SSD1306_WHITE);
  switch (page) {
    case 0:
      display.setCursor(2, 17);
      display.println("WIRING PINOUT:");
      display.println("OLED: SDA=8, SCL=9");
      display.println("SPI: SCK=4,MI=5,MO=6");
      display.println("R1: CE=20, CS=21");
      display.println("R2: CE=7,  CS=10");
      break;
    case 1:
      display.setCursor(2, 17);
      display.println("NRF CHANNELS:");
      display.println("WiFi 1/6/11=12/37/62");
      display.println("BLE adv = 2/26/80");
      display.println("BT classic = 2..80");
      display.println("Drone = 0..124");
      break;
    case 2:
      display.setCursor(2, 17);
      display.println("TARGETS:");
      display.println("All: full range");
      display.println("Audio: BT speaker");
      display.println("HID: mouse/keyboard");
      display.println("Beacon: BLE tags");
      break;
    case 3:
      display.setCursor(2, 17);
      display.println("AUTO MODE:");
      display.println("Scans RPD energy");
      display.println("Thr filters noise");
      display.println("Jams active ch only");
      display.println("Slower but smart");
      break;
    case 4:
      display.setCursor(2, 17);
      display.println("PRESETS:");
      display.println("14 curated lists");
      display.println("WiFi/BLE/BT/Drone");
      display.println("USB/FPV/RC/Zigbee");
      display.println("Select + Save");
      break;
    case 5:
      display.setCursor(2, 17);
      display.println("HARDWARE:");
      display.println("NRF VCC: 3.3V ONLY");
      display.println("Cap: 47uF on NRF");
      display.println("I2C pullups: 4.7k");
      display.println("For lab testing");
      break;
  }
  display.drawLine(0, 53, SCREEN_WIDTH, 53, SSD1306_WHITE);
  display.setCursor(2, 55);
  display.print("UP/DN:Page  SEL:Exit");
  display.display();
}

void handleHelpSelection() {
  static bool btnPressed = false;
  if (!btnPressed) {
    if (isButtonPressed(UP_BUTTON_PIN)) {
      currentHelpPage = (currentHelpPage == 0) ? (NUM_HELP_PAGES - 1) : (currentHelpPage - 1);
      drawHelpScreen(currentHelpPage); btnPressed = true;
    } else if (isButtonPressed(DOWN_BUTTON_PIN)) {
      currentHelpPage = (currentHelpPage + 1) % NUM_HELP_PAGES;
      drawHelpScreen(currentHelpPage); btnPressed = true;
    } else if (isButtonPressed(SELECT_BUTTON_PIN)) {
      btnPressed = true;
      currentState = STATE_MENU;
      drawMenu();
      nonBlockingDelay(300);
    }
  } else {
    if (!isButtonPressed(UP_BUTTON_PIN) && !isButtonPressed(DOWN_BUTTON_PIN) && !isButtonPressed(SELECT_BUTTON_PIN)) {
      btnPressed = false;
      if (LED_PIN < 40) digitalWrite(LED_PIN, LOW);
    }
  }
}

// ===================== MENU HANDLING =====================
void handleMenuSelection() {
  static bool btnPressed = false;
  if (!btnPressed) {
    if (isButtonPressed(UP_BUTTON_PIN)) {
      selectedMenuItem = static_cast<MenuItem>((selectedMenuItem == 0) ? (NUM_MENU_ITEMS - 1) : (selectedMenuItem - 1));
      if (selectedMenuItem == (NUM_MENU_ITEMS - 1)) firstVisibleMenuItem = NUM_MENU_ITEMS - 2;
      else if (selectedMenuItem < firstVisibleMenuItem) firstVisibleMenuItem = selectedMenuItem;
      drawMenu(); btnPressed = true;
    } else if (isButtonPressed(DOWN_BUTTON_PIN)) {
      selectedMenuItem = static_cast<MenuItem>((selectedMenuItem + 1) % NUM_MENU_ITEMS);
      if (selectedMenuItem == 0) firstVisibleMenuItem = 0;
      else if (selectedMenuItem >= (firstVisibleMenuItem + 2)) firstVisibleMenuItem = selectedMenuItem - 1;
      drawMenu(); btnPressed = true;
    } else if (isButtonPressed(SELECT_BUTTON_PIN)) {
      executeSelectedMenuItem(); btnPressed = true;
    }
  } else {
    if (!isButtonPressed(UP_BUTTON_PIN) && !isButtonPressed(DOWN_BUTTON_PIN) && !isButtonPressed(SELECT_BUTTON_PIN)) {
      btnPressed = false;
      if (LED_PIN < 40) digitalWrite(LED_PIN, LOW);
    }
  }
}

void executeSelectedMenuItem() {
  switch (selectedMenuItem) {
    case BT_JAM: {
      currentState = STATE_BT_JAM;
      displayInfo("BT/BLE JamX", "ACTIVATING", "Starting...");
      initRadios();
      nonBlockingDelay(1200);
      char line3[24];
      if (config.autoChannel) snprintf(line3, sizeof(line3), "Auto ON");
      else if (config.usePresets) snprintf(line3, sizeof(line3), "Preset: %s", PRESETS[config.presetIndex].name);
      else snprintf(line3, sizeof(line3), "Manual");
      displayInfo("BT/BLE JamX", "ACTIVE", line3, "[SEL] Stop");
      while (!isButtonPressed(SELECT_BUTTON_PIN)) jamStep();
      recoverDisplay();
      currentState = STATE_MENU;
      drawMenu();
      nonBlockingDelay(400);
      break;
    }
    case WIFI_JAM: {
      currentState = STATE_WIFI_JAM;
      displayInfo("WIFI JamX", "ACTIVATING", "Starting...");
      initRadios();
      nonBlockingDelay(1200);
      displayInfo("WIFI JamX", "ACTIVE", config.autoChannel ? "Auto ON" : "Manual", "[SEL] Stop");
      while (!isButtonPressed(SELECT_BUTTON_PIN)) jamStep();
      recoverDisplay();
      currentState = STATE_MENU;
      drawMenu();
      nonBlockingDelay(400);
      break;
    }
    case DRONE_JAM: {
      currentState = STATE_DRONE_JAM;
      displayInfo("DRONE JamX", "ACTIVATING", "Starting...");
      initRadios();
      nonBlockingDelay(1200);
      displayInfo("DRONE JamX", "ACTIVE", config.autoChannel ? "Auto ON" : "Manual", "[SEL] Stop");
      while (!isButtonPressed(SELECT_BUTTON_PIN)) jamStep();
      recoverDisplay();
      currentState = STATE_MENU;
      drawMenu();
      nonBlockingDelay(400);
      break;
    }
    case MULTI_JAM: {
      currentState = STATE_MULTI_JAM;
      displayInfo("MultiCH JamX", "ACTIVATING", "Starting...");
      initRadios();
      nonBlockingDelay(1200);
      displayInfo("MultiCH JamX", "ACTIVE", "All bands", "[SEL] Stop");
      while (!isButtonPressed(SELECT_BUTTON_PIN)) jamStep();
      recoverDisplay();
      currentState = STATE_MENU;
      drawMenu();
      nonBlockingDelay(400);
      break;
    }
    case SPECTRUM_SCAN: {
      currentState = STATE_SPECTRUM_SCAN;
      displayInfo("SPECTRUM SCAN", "Scanning...", "Please wait");
      spiBus.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
      scanChannelsDual();
      drawSpectrum();
      nonBlockingDelay(500);
      while (!isButtonPressed(SELECT_BUTTON_PIN)) {
        if (isButtonPressed(UP_BUTTON_PIN) || isButtonPressed(DOWN_BUTTON_PIN)) {
          scanChannelsDual();
          drawSpectrum();
          nonBlockingDelay(300);
        }
      }
      currentState = STATE_MENU;
      drawMenu();
      nonBlockingDelay(400);
      break;
    }
    case TEST_RADIOS:
      currentState = STATE_TEST_RADIOS;
      displayInfo("TEST RADIOS", "Testing...");
      testRadios();
      nonBlockingDelay(1500);
      break;
    case SETTINGS:
      currentState = STATE_SETTINGS;
      selectedSettingsItem = 0;
      firstVisibleSettingsItem = 0;
      drawSettingsMenu();
      break;
    case HELP:
      currentState = STATE_HELP;
      currentHelpPage = 0;
      drawHelpScreen(currentHelpPage);
      break;
  }
}

// ===================== SPLASH =====================
void displayTitleScreen() {
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_adventurer_tr);
  u8g2_for_adafruit_gfx.setCursor(20, 40);
  u8g2_for_adafruit_gfx.print("JamX BoX");
  display.display();
}

void displayInfoScreen() {
  display.clearDisplay();
  u8g2_for_adafruit_gfx.setFont(u8g2_font_baby_tf);
  u8g2_for_adafruit_gfx.setCursor(0, 22);
  u8g2_for_adafruit_gfx.print("Welcome to 2.4GHZ JAMR!");
  u8g2_for_adafruit_gfx.setCursor(0, 30);
  u8g2_for_adafruit_gfx.print("Dual NRF24 + RSSI Scan");
  u8g2_for_adafruit_gfx.setCursor(0, 38);
  u8g2_for_adafruit_gfx.print("14 Presets + Auto mode");
  u8g2_for_adafruit_gfx.setCursor(0, 54);
  u8g2_for_adafruit_gfx.print("Lab testing only ~_~");
  display.display();
}

// ===================== SETUP =====================
void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  delay(300);

  Serial.begin(115200);
  delay(10);

  initDisplay();

  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();

  pinMode(UP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SELECT_BUTTON_PIN, INPUT_PULLUP);
  if (LED_PIN < 40) pinMode(LED_PIN, OUTPUT);

  u8g2_for_adafruit_gfx.begin(display);

  if (!SPIFFS.begin(true)) Serial.println("SPIFFS Mount Failed!");
  loadSettings();

  displayTitleScreen();
  delay(1000);
  displayInfoScreen();
  delay(1500);

  initRadios();
  delay(1200);
  recoverDisplay();
  drawMenu();
}

// ===================== LOOP =====================
void loop() {
  switch (currentState) {
    case STATE_MENU:      handleMenuSelection(); break;
    case STATE_BT_JAM:
    case STATE_WIFI_JAM:
    case STATE_DRONE_JAM:
    case STATE_MULTI_JAM:
    case STATE_TEST_RADIOS:
    case STATE_SPECTRUM_SCAN:
      if (isButtonPressed(SELECT_BUTTON_PIN)) {
        recoverDisplay();
        currentState = STATE_MENU;
        drawMenu();
        nonBlockingDelay(400);
        return;
      }
      break;
    case STATE_SETTINGS:  handleSettingsSelection(); break;
    case STATE_HELP:      handleHelpSelection(); break;
  }
}