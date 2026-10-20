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

// nRF24L01+ synthesizer settling time.  After a channel change the PLL needs
// roughly 130 us before the carrier is on-frequency.  A constant-carrier jammer
// that retunes faster than this NEVER locks, so it radiates nothing usable -
// which is why a dwell of 0 makes every "sweep" jammer silent.
#define NRF24_MIN_DWELL_US 200

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

// Joystick pin definitions
// SW:   GPIO 25 (Digital push switch, active-LOW with INPUT_PULLUP)
// VRy:  GPIO 35 (ADC1_CH7, analog input)
// VRx:  GPIO 34 (ADC1_CH6, analog input)
#define JOY_SW           25
#define JOY_VRY          35
#define JOY_VRX          34

// User-configurable Joystick settings (persisted in flash)
inline int joy_inverted  = 1;   // 1 = Inverted (hardware default), 0 = Normal
inline int joy_hold_ms   = 350; // Milliseconds before hold-to-repeat starts
inline int joy_repeat_ms = 140; // Auto-repeat interval while holding
inline int joy_deadzone  = 800; // Deadzone threshold around center (0..2048)

// Responsive Joystick Axis controller with instant flick response & hold-to-repeat
class JoystickAxis
{
public:
  JoystickAxis() : _state(false), _triggered(false), _held(false), _holdTimer(0), _repeatTimer(0) {}

  void tick(bool active)
  {
    uint32_t now = millis();
    if ( active )
    {
      if ( !_state )
      {
        // First frame of deflection: trigger immediately! No delay!
        _state       = true;
        _triggered   = true;
        _held        = false;
        _holdTimer   = now;
        _repeatTimer = now;
      }
      else
      {
        // Still being held deflected
        if ( now - _holdTimer >= (uint32_t)joy_hold_ms )
        {
          _held = true;
          if ( now - _repeatTimer >= (uint32_t)joy_repeat_ms )
          {
            _triggered   = true; // Fire repeated click!
            _repeatTimer = now;
          }
        }
      }
    }
    else
    {
      // Released back to deadzone
      _state     = false;
      _held      = false;
    }
  }

  bool isSingle()
  {
    if ( _triggered )
    {
      _triggered = false;
      return true;
    }
    return false;
  }

  bool isClick()  { return isSingle(); }
  bool isHold()   { return _held; }
  bool isHolded() { return _held; }
  bool state()    { return _state; }

  void resetStates()
  {
    _triggered = false;
    _held      = false;
  }

  // Compatibility stubs for GButton API
  void setTickMode(bool) {}
  void setClickTimeout(uint16_t) {}
  void setTimeout(uint16_t) {}
  void setDebounce(uint16_t) {}

private:
  bool     _state;
  bool     _triggered;
  bool     _held;
  uint32_t _holdTimer;
  uint32_t _repeatTimer;
};

inline SPIClass        *hp = nullptr;
inline GButton          btnOK(JOY_SW, HIGH_PULL, NORM_OPEN);
inline JoystickAxis     btnNext;
inline JoystickAxis     btnPrevious;

inline WebServer        server(80);
inline DNSServer        dnsServer;
inline Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT_MAX, &Wire, OLED_RESET);
inline Preferences      prefs;

// Joystick resting center calibration values
inline int joy_center_y = 2048;
inline int joy_center_x = 2048;

// Configures the joystick pins and samples resting center position robustly
inline void initJoystick()
{
  pinMode(JOY_SW, INPUT_PULLUP);
  analogReadResolution(12); // 12-bit ADC (0..4095)

  // Give power rails and ADC reference voltage time to stabilize on hard reboot
  delay(120);

  // Warm up ADC and discard initial transient reads
  for ( int i = 0; i < 4; i++ )
  {
    analogRead(JOY_VRY);
    analogRead(JOY_VRX);
    delay(2);
  }

  // Calibrate joystick resting center at boot (average 32 samples with noise rejection)
  long sum_y = 0;
  long sum_x = 0;
  int  min_y = 4096, max_y = 0;
  int  min_x = 4096, max_x = 0;

  for ( int i = 0; i < 32; i++ )
  {
    int vy = analogRead(JOY_VRY);
    int vx = analogRead(JOY_VRX);

    sum_y += vy;
    sum_x += vx;

    if ( vy < min_y ) min_y = vy;
    if ( vy > max_y ) max_y = vy;
    if ( vx < min_x ) min_x = vx;
    if ( vx > max_x ) max_x = vx;

    delay(3);
  }

  // If the user was wiggling the joystick or readings were unstable, use safe default 2048
  if ( (max_y - min_y) > 350 || (max_x - min_x) > 350 )
  {
    joy_center_y = 2048;
    joy_center_x = 2048;
  }
  else
  {
    joy_center_y = sum_y / 32;
    joy_center_x = sum_x / 32;
  }

  // Strict sanity bounds for hardware center (potentiometer middle on 3.3V ADC is ~1700-2400)
  if ( joy_center_y < 1600 || joy_center_y > 2500 ) joy_center_y = 2048;
  if ( joy_center_x < 1600 || joy_center_x > 2500 ) joy_center_x = 2048;

  btnNext.resetStates();
  btnPrevious.resetStates();
}

// Reads joystick SW (digital) and deflection on VRy/VRx (analog with deadband)
inline void updateJoystickButtons()
{
  // SW: digital active-LOW button
  bool sw_pressed = (digitalRead(JOY_SW) == LOW);
  btnOK.tick(sw_pressed);

  int raw_y = analogRead(JOY_VRY);
  int raw_x = analogRead(JOY_VRX);

  bool next_active = false;
  bool prev_active = false;

  if ( joy_inverted )
  {
    // Inverted: Low values on axes mean Next, High values mean Previous
    next_active = (raw_y < (joy_center_y - joy_deadzone)) || (raw_x < (joy_center_x - joy_deadzone));
    prev_active = (raw_y > (joy_center_y + joy_deadzone)) || (raw_x > (joy_center_x + joy_deadzone));
  }
  else
  {
    // Normal: High values on axes mean Next, Low values mean Previous
    next_active = (raw_y > (joy_center_y + joy_deadzone)) || (raw_x > (joy_center_x + joy_deadzone));
    prev_active = (raw_y < (joy_center_y - joy_deadzone)) || (raw_x < (joy_center_x - joy_deadzone));
  }

  // Runaway watchdog: if an axis is continuously held active for over 15s without SW click,
  // it indicates a disconnected analog line or corrupted center -> auto-recenter to current
  static uint32_t s_activeTimer = 0;
  uint32_t now = millis();
  if ( next_active || prev_active )
  {
    if ( s_activeTimer == 0 ) s_activeTimer = now;
    else if ( now - s_activeTimer > 15000 )
    {
      // Auto-recenter to unfreeze
      joy_center_y = raw_y;
      joy_center_x = raw_x;
      next_active = false;
      prev_active = false;
      btnNext.resetStates();
      btnPrevious.resetStates();
      s_activeTimer = 0;
    }
  }
  else
  {
    s_activeTimer = 0;
  }

  btnNext.tick(next_active);
  btnPrevious.tick(prev_active);
}

#endif