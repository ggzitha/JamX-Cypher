#ifndef JAM_H
#define JAM_H

#include "attack.h"
#include "config.h"
#include "scan.h"
#include "serial.h"

void InitRadios();
void DeinitRadios(bool stopConstCarrier);
void HSPI_init();
void HSPI_initBare();
void HSPI_deinit();

// Restores every setting written by handlerSetSettings() from NVS.  Without
// this the device boots with nrf_pa = 0 (-18 dBm), jam_delay = 0 and
// SCREEN_HEIGHT = 0, so jammers emit almost nothing and the OLED stays blank.
void loadSettings();

// Set by the "stop" serial / web command to end a running jam.
extern bool stopJam;

// RF self-test: module 0 transmits a constant carrier, modules 1..n-1 report
// carrier detect.  Returns true when the RF path is verified working.
bool rfTest(int *detected, int *total);

// True while a jam task owns the radios.
bool jamIsActive();

// Existing jam functions
void bluetooth_jam();
void drone_jam();
void ble_advertising_jam();
void ble_data_jam();
void wifi_jam();
void wifi_scan_jam();
void wifi_channel();
void zigbee_jam();
void misc_jam();
void full_24g_jam();
void usb_24g_jam();
void fpv_video_jam();

// HID / Wireless Peripheral jammers (nRF24 radio)
void hid_wireless_jam();        // Generic 2.4 GHz HID dongle sweep (ch 2-72 even)
void logitech_unifying_jam();   // Logitech Unifying / Bolt targeted 25-channel set
void ms_wireless_jam();         // Microsoft 2.4 GHz wireless HID (ch 2-80)

// RC / hobby controller jammers (nRF24 radio)
void rc_controller_jam();       // Full-band RC sweep ch 0-125
void flysky_jam();              // FlySky AFHDS-2A hop channels
void spektrum_jam();            // Spektrum DSM2/DSMX (ch 0-79)
void frsky_jam();               // FrSky D8/D16/ACCESS (ch 0-100)

// BLE attack wrappers (bridge to ble_attack.cpp, run as FreeRTOS task)
void ble_apple_flood_task();    // Apple Continuity BLE flood
void ble_adv_flood_task();      // Generic BLE advertisement flood
void ble_scan_flood_task();     // BLE scan + advertise flood

bool jamHandler(void (*action)(), String htmlResponse = "", const unsigned char *bitmap = nullptr, bool CW = false, bool mode = false, bool isMisc = false, bool scan = false, int ch1 = 0, int ch2 = 0);

void updateDisplay(int menuNum);

// nRF24 detection / status (defined in nRF24_jammer.cpp)
int  detectNRF24Modules(int *found_ce, int *found_csn);
bool checkNRF24Module(int ce, int csn);
void nrfStatusScreen();

#endif