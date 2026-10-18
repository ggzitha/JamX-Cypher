#ifndef CONFIG_H
#define CONFIG_H

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "DNSServer.h"
#include "GyverButton.h"
#include "NimBLEDevice.h"
#include "Preferences.h"
#include "RF24.h"
#include "Update.h"
#include "WebServer.h"
#include "WiFi.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT_MAX 64
inline int SCREEN_HEIGHT;
#define OLED_RESET -1

#define default_ssid "jammer"
#define default_password "illuminati"
#define Version_Number "V3.5.0"
#define Version_Name "Flexible"

inline String ssid;
inline String password;

inline int menu_number;
inline int baudrate;
inline int display_setting;
inline int logo;
inline int access_point;
inline int buttons;
inline int Separate_or_together;

inline int bluetooth_jam_method;
inline int drone_jam_method;
inline int wifi_jam_method;
inline int misc_jam_method;
inline int nrf_pa;

inline byte       bluetooth_channels[] = {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
inline byte       ble_channels[]       = {2, 26, 80};
inline const char jam_text[]           = "xxxxxxxxxxxxxxxx";
inline int        jam_delay;

inline String BLE_MAC[100];
inline int    BLE_RSSI[100];
inline String BLE_Name[100];
inline String BLE_ManufacturerData[100];
inline String BLE_Payload[100];

inline int   nrf24_count;
inline int   ce_pins[30];
inline int   csn_pins[30];
inline RF24 *radios[30];

inline bool webServerStarted = false;
inline bool dnsServerStarted = false;

inline char   SSIDs_Array[100][33];
inline String APs_array[14];
inline int    WiFiScanChannels[14];
inline int    NumberChannels;
inline char   default_SSIDs_Array[100][33] = {
    "0 Never gonna give you up",
    "1 Never gonna let you down",
    "2 Never gonna run around",
    "3 And desert you",
    "4 Never gonna make you cry",
    "5 Never gonna say goodbye",
    "6 Never gonna tell a lie",
    "7 And hurt you",
};

inline int  channel1 = 0;
inline int  channel2 = 0;
inline int  flag     = 0;
inline bool hspi     = false;

// Joystick pins
// SW  = 25  (digital, INPUT_PULLUP — btnOK / execute)
// VRy = 35  (ADC1, analog — pushed down  -> btnNext)
// VRx = 34  (ADC1, analog — pushed up    -> btnPrevious)
// NOTE: 34/35 are ADC1 pins so they are NOT blocked when Wi-Fi is active.
//       26/27 are ADC2 and CANNOT be used for analogRead while Wi-Fi is running.
#define JOY_SW  25
#define JOY_VRY 35   // down -> btnNext
#define JOY_VRX 34   // up   -> btnPrevious

inline SPIClass        *hp = nullptr;
inline GButton          btnOK(JOY_SW);
inline GButton          btnNext(JOY_VRY);
inline GButton          btnPrevious(JOY_VRX);

inline WebServer        server(80);
inline DNSServer        dnsServer;
inline Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT_MAX, &Wire, OLED_RESET);
inline Preferences      prefs;

// ---- Joystick helper ----
// Replaces the three btnXXX.tick() calls. Reads the joystick's SW (digital) and
// VRx/VRy (analog) and drives the GButton state machines via tick(bool).
//   SW  (pin 25) -> btnOK
//   VRy (pin 26) -> pushed down  -> btnNext
//   VRx (pin 27) -> pushed up    -> btnPrevious
//   Left/Right are unused for now.
inline void updateJoystickButtons()
{
  // SW button: digitalRead returns LOW when pressed (INPUT_PULLUP).
  bool sw_pressed = (digitalRead(JOY_SW) == LOW);
  btnOK.tick(sw_pressed);

  // Analog axes: 12-bit ADC, 0..4095.
  // Centre ~2048; threshold at ~600 (well below centre) to detect deflection.
  static const int JOY_THRESHOLD = 600;

  int raw_down  = analogRead(JOY_VRY); // pushed down -> high value
  int raw_up    = analogRead(JOY_VRX); // pushed up   -> high value
  // Left/Right would use the other axis endpoints but are unused for now.

  bool down_active = (raw_down >= JOY_THRESHOLD);
  bool up_active   = (raw_up   >= JOY_THRESHOLD);

  // Feed the GButton state machines with the current (debounced by GButton)
  // pressed state.  GButton's internal debounce + click detection handles the
  // rest, exactly as it did for real buttons.
  btnNext.tick(down_active);
  btnPrevious.tick(up_active);
}

// Called once in setup() to configure the joystick pins.
inline void initJoystick()
{
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12); // 12-bit ADC (0..4095)
}

#endif