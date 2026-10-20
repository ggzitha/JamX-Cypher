#include "bitmap.h"
#include "config.h"
#include "html.h"
#include "jam.h"
#include "scan.h"
#include "serial.h"

static String jsonEscape(const String &s)
{
  String out;
  out.reserve(s.length() + 8);
  for ( size_t i = 0; i < s.length(); i++ )
  {
    char c = s[i];
    switch ( c )
    {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

void handleRoot()
{
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)html, html_len);
}

void HandleWebCommand()
{
  String command = server.arg("command");
  command.trim();
  command.toLowerCase();
  CommandsHandler(command, true);
}

// ---------------------------------------------------------------------------
// Unified PSP-style menu framework (shared by main menu and every submenu)
// ---------------------------------------------------------------------------
enum {
  ICON_BT, ICON_DRONE, ICON_WIFI, ICON_BLE, ICON_ZIGBEE, ICON_MISC,
  ICON_FULL, ICON_USB, ICON_FPV, ICON_HID, ICON_RC, ICON_APPLE,
  ICON_SETTINGS, ICON_TOOLS, ICON_SCAN, ICON_REBOOT, ICON_RF
};

static void drawMenuIcon(int x, int y, int id, uint16_t color)
{
  switch ( id )
  {
  case ICON_WIFI: // 3 rising bars
    display.fillRect(x + 1, y + 6, 2, 3, color);
    display.fillRect(x + 5, y + 3, 2, 6, color);
    display.fillRect(x + 9, y + 0, 2, 9, color);
    break;
  case ICON_BT: // circle + centre
    display.drawCircle(x + 5, y + 5, 4, color);
    display.drawCircle(x + 5, y + 5, 1, color);
    break;
  case ICON_BLE: // arrow
    display.fillTriangle(x + 1, y + 5, x + 11, y + 1, x + 11, y + 9, color);
    break;
  case ICON_ZIGBEE:
    display.drawLine(x + 1, y + 2, x + 5, y + 7, color);
    display.drawLine(x + 5, y + 7, x + 9, y + 2, color);
    break;
  case ICON_DRONE: // X
    display.drawLine(x + 1, y + 1, x + 9, y + 9, color);
    display.drawLine(x + 9, y + 1, x + 1, y + 9, color);
    break;
  case ICON_HID: // keyboard dots
    display.fillCircle(x + 2, y + 3, 1, color);
    display.fillCircle(x + 5, y + 3, 1, color);
    display.fillCircle(x + 8, y + 3, 1, color);
    display.fillRect(x + 3, y + 7, 6, 2, color);
    break;
  case ICON_RC: // stick
    display.drawCircle(x + 4, y + 4, 3, color);
    display.fillCircle(x + 4, y + 4, 1, color);
    display.drawLine(x + 4, y + 7, x + 4, y + 9, color);
    break;
  case ICON_USB: // plug
    display.drawRect(x + 2, y + 1, 6, 5, color);
    display.fillRect(x + 5, y + 5, 2, 4, color);
    break;
  case ICON_APPLE:
    display.fillCircle(x + 5, y + 3, 3, color);
    display.fillCircle(x + 4, y + 6, 2, color);
    break;
  case ICON_SETTINGS: // gear
    display.drawCircle(x + 5, y + 5, 3, color);
    display.drawLine(x + 5, y, x + 5, y + 9, color);
    display.drawLine(x + 1, y + 5, x + 9, y + 5, color);
    break;
  case ICON_FULL: // bars with gap
    display.fillRect(x + 1, y, 2, 9, color);
    display.fillRect(x + 5, y, 2, 9, color);
    display.fillRect(x + 9, y, 2, 9, color);
    display.drawLine(x + 2, y + 4, x + 9, y + 4, BLACK);
    break;
  case ICON_FPV:
    display.drawRect(x + 1, y + 1, 10, 7, color);
    display.drawLine(x + 6, y + 7, x + 6, y + 9, color);
    break;
  case ICON_SCAN:
    display.drawCircle(x + 3, y + 3, 4, color);
    display.drawLine(x + 7, y + 7, x + 10, y + 10, color);
    break;
  case ICON_REBOOT:
    display.drawCircle(x + 5, y + 5, 4, color);
    display.fillRect(x + 4, y, 3, 3, BLACK);
    break;
  case ICON_RF: // wave
    display.drawLine(x + 1, y + 8, x + 4, y + 4, color);
    display.drawLine(x + 4, y + 4, x + 7, y + 8, color);
    display.drawLine(x + 7, y + 8, x + 10, y + 4, color);
    break;
  default: // ICON_MISC / generic
    display.drawCircle(x + 4, y + 4, 3, color);
    display.drawLine(x + 7, y + 7, x + 10, y + 7, color);
    break;
  }
}

static void drawMenuFrame(const char *title)
{
  display.fillRect(0, 0, SCREEN_WIDTH, 12, BLACK);
  display.drawFastHLine(0, 12, SCREEN_WIDTH, WHITE);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(3, 2);
  display.print(title);
}

static void drawMenuList(const char *const labels[], const int *icons, int count, int current)
{
  const int firstY     = 14;
  const int rowH       = 9;
  const int maxVisible = (SCREEN_HEIGHT == 64) ? 4 : 2;
  int       scroll     = current - (maxVisible / 2);
  if ( scroll < 0 )
    scroll = 0;
  if ( scroll + maxVisible > count )
    scroll = count - maxVisible;

  display.setTextSize(1);
  for ( int i = 0; i < maxVisible; i++ )
  {
    int  idx = scroll + i;
    if ( idx >= count )
      break;
    int  y   = firstY + i * rowH;
    bool sel = (idx == current);
    if ( sel )
    {
      display.fillRect(0, y - 1, SCREEN_WIDTH, rowH, WHITE);
      display.setTextColor(BLACK, WHITE);
    }
    else
      display.setTextColor(WHITE, BLACK);

    if ( icons != nullptr )
      drawMenuIcon(3, y, icons[idx], sel ? BLACK : WHITE);

    display.setCursor(17, y);
    int len = strlen(labels[idx]);
    if ( sel && len > 17 )
    {
      // Marquee horizontal scroll when long label is selected / hovered
      int maxOffset = len - 17;
      int step = (millis() / 250) % (maxOffset + 6);
      int offset = 0;
      if ( step > 2 )
      {
        offset = step - 2;
        if ( offset > maxOffset ) offset = maxOffset;
      }
      char buf[20];
      strncpy(buf, labels[idx] + offset, 17);
      buf[17] = '\0';
      display.print(buf);
    }
    else
    {
      char buf[20];
      strncpy(buf, labels[idx], 17);
      buf[17] = '\0';
      display.print(buf);
    }
  }
}

static void drawMenuHint()
{
  const int y = SCREEN_HEIGHT - 9;
  display.fillRect(0, y, SCREEN_WIDTH, 9, BLACK);
  display.drawFastHLine(0, y, SCREEN_WIDTH, WHITE);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(2, y + 1);
  display.print("OK select   Hold back");
}

static const char *const MENU_SHORT[14] = {
    "BT Jammer", "Drone", "WiFi", "BLE", "Zigbee", "Misc",
    "Settings", "Full 2.4G", "USB 2.4G", "FPV", "HID", "RC",
    "Apple BLE", "Tools"
};
static const char *const MENU_NAME[14] = {
    "Bluetooth Jammer", "Drone Jammer", "WiFi Attacks", "BLE Jammer",
    "Zigbee Jammer", "Misc Jammer", "Settings", "Full 2.4 GHz",
    "USB 2.4 GHz", "FPV / Video", "HID Jammer", "RC Jammer",
    "Apple BLE Flood", "Tools"
};
static const int MENU_ICON[14] = {
    ICON_BT, ICON_DRONE, ICON_WIFI, ICON_BLE, ICON_ZIGBEE, ICON_MISC,
    ICON_SETTINGS, ICON_FULL, ICON_USB, ICON_FPV, ICON_HID, ICON_RC,
    ICON_APPLE, ICON_TOOLS
};

void updateDisplay(int menuNum)
{
  const int ITEM_COUNT = 14;

  display.clearDisplay();

  // 1. Draw central large creative logo (scaled & centered for ALL modules 0..13 so nothing is cropped)
  drawLargeModuleLogo(menuNum, SCREEN_WIDTH / 2, 32);

  // 2. Title Bar: Overlaid on top center in Bold
  display.fillRect(0, 0, SCREEN_WIDTH, 11, BLACK);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  int w = strlen(MENU_NAME[menuNum]) * 6;
  int titleX = (SCREEN_WIDTH - w) / 2;
  if ( titleX < 2 ) titleX = 2;
  display.setCursor(titleX, 2);
  display.print(MENU_NAME[menuNum]);
  display.setCursor(titleX + 1, 2); // 1-pixel horizontal bold overlay
  display.print(MENU_NAME[menuNum]);
  display.drawFastHLine(0, 11, SCREEN_WIDTH, WHITE);

  // 3. Bottom Bar: Separator line + < [dots] >
  const int barY = SCREEN_HEIGHT - 11;
  display.drawFastHLine(0, barY, SCREEN_WIDTH, WHITE);
  display.fillRect(0, barY + 1, SCREEN_WIDTH, 10, BLACK);

  // Left arrow '<'
  display.drawLine(4, barY + 3, 1, barY + 5, WHITE);
  display.drawLine(1, barY + 5, 4, barY + 7, WHITE);
  display.drawLine(5, barY + 3, 2, barY + 5, WHITE);
  display.drawLine(2, barY + 5, 5, barY + 7, WHITE);

  // Right arrow '>'
  display.drawLine(SCREEN_WIDTH - 5, barY + 3, SCREEN_WIDTH - 2, barY + 5, WHITE);
  display.drawLine(SCREEN_WIDTH - 2, barY + 5, SCREEN_WIDTH - 5, barY + 7, WHITE);
  display.drawLine(SCREEN_WIDTH - 6, barY + 3, SCREEN_WIDTH - 3, barY + 5, WHITE);
  display.drawLine(SCREEN_WIDTH - 3, barY + 5, SCREEN_WIDTH - 6, barY + 7, WHITE);

  // Dotted page indicator (filled = selected, empty dot = unselected)
  int dotY = barY + 5;
  for ( int i = 0; i < ITEM_COUNT; i++ )
  {
    int cx = 12 + i * 8;
    if ( i == menuNum )
    {
      display.fillCircle(cx, dotY, 2, WHITE); // Filled ●
    }
    else
    {
      display.drawPixel(cx, dotY, WHITE);     // Unselected dot ○
    }
  }

  display.display();
}

void handlerSetSettings()
{
  baudrate             = server.arg("baudrate").toInt();
  logo                 = server.arg("logo").toInt();
  display_setting      = server.arg("display").toInt();
  SCREEN_HEIGHT        = server.arg("screen_height").toInt();
  buttons              = server.arg("button").toInt();
  Separate_or_together = server.arg("sweep").toInt();
  nrf_pa               = server.arg("pa").toInt();
  bluetooth_jam_method = server.arg("bt_method").toInt();
  drone_jam_method     = server.arg("drone_method").toInt();
  misc_jam_method      = server.arg("misc_method").toInt();
  ssid                 = server.arg("ssid");
  password             = server.arg("password");
  access_point         = server.arg("ap").toInt();
  jam_delay            = server.arg("delay").toInt();

  // Never persist values that would silently break the device: a screen
  // height other than 32/64 renders nothing and a PA index outside 0..3
  // maps to an undefined rf24_pa_dbm_e.
  if ( SCREEN_HEIGHT != 32 && SCREEN_HEIGHT != 64 )
    SCREEN_HEIGHT = 64;
  if ( nrf_pa < 0 || nrf_pa > 3 )
    nrf_pa = 3;
  if ( jam_delay < 0 )
    jam_delay = 0;
  if ( jam_delay > 10000 )
    jam_delay = 10000;
  if ( Separate_or_together < 0 || Separate_or_together > 1 )
    Separate_or_together = 0;
  if ( access_point < 0 || access_point > 1 )
    access_point = 0;

  String beacon_list = server.arg("beacon");
  String ce_list     = server.arg("ce");
  String csn_list    = server.arg("csn");

  auto parseTokens = [](const String &str, auto callback, int maxCount) -> int {
    if ( str.length() == 0 )
      return 0;

    int count = 0;
    int start = 0;
    while ( count < maxCount )
    {
      int sep = str.indexOf('|', start);
      if ( sep == -1 )
        sep = str.length();

      String token = str.substring(start, sep);
      token.trim();

      if ( token.length() > 0 )
      {
        callback(token, count++);
      }

      if ( sep == str.length() )
        break;
      start = sep + 1;
    }
    return count;
  };

  nrf24_count = parseTokens(ce_list, [](const String &token, int idx) { ce_pins[idx] = token.toInt(); }, 30);

  parseTokens(csn_list, [](const String &token, int idx) { csn_pins[idx] = token.toInt(); }, 30);

  memset(SSIDs_Array, 0, sizeof(SSIDs_Array));
  parseTokens(beacon_list, [](const String &token, int idx) {
    String ssid_str = token.length() > 31 ? token.substring(0, 31) : token;
    strlcpy(SSIDs_Array[idx], ssid_str.c_str(), sizeof(SSIDs_Array[idx])); }, 100);

  prefs.putInt("baudrate", baudrate);
  prefs.putInt("logo_configs", logo);
  prefs.putInt("display_configs", display_setting);
  prefs.putInt("screen_height", SCREEN_HEIGHT);
  prefs.putInt("buttons_configs", buttons);
  prefs.putInt("SorT_configs", Separate_or_together);
  prefs.putInt("PA_configs", nrf_pa);
  prefs.putInt("bt_configs", bluetooth_jam_method);
  prefs.putInt("drone_configs", drone_jam_method);
  prefs.putInt("misc_configs", misc_jam_method);
  prefs.putInt("nRF24_count", nrf24_count);
  prefs.putString("ssid", ssid);
  prefs.putString("password", password);
  prefs.putInt("AP_configs", access_point);
  prefs.putInt("jam_delay", jam_delay);

  prefs.putBytes("SSIDs_Array", SSIDs_Array, sizeof(SSIDs_Array));
  prefs.putBytes("nRF24_ce_pins", ce_pins, sizeof(ce_pins));
  prefs.putBytes("nRF24_csn_pins", csn_pins, sizeof(csn_pins));

  Serial.updateBaudRate(baudrate);

  if ( SCREEN_HEIGHT == 32 )
  {
    display.ssd1306_command(SSD1306_SETCOMPINS);
    display.ssd1306_command(0x02);
    display.ssd1306_command(SSD1306_SETMULTIPLEX);
    display.ssd1306_command(0x1F);
  }
  else
  {
    display.ssd1306_command(SSD1306_SETCOMPINS);
    display.ssd1306_command(0x12);
    display.ssd1306_command(SSD1306_SETMULTIPLEX);
    display.ssd1306_command(0x3F);
  }

  server.send(200, "text/plain", "OK");
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------
// handlerSetSettings() writes every one of these keys, but nothing ever read
// them back.  After a power cycle the device therefore ran with nrf_pa = 0
// (RF24_PA_MIN, -18 dBm), jam_delay = 0 and SCREEN_HEIGHT = 0 (which makes
// every drawBitmap() call render nothing).  This restores them all.
// ---------------------------------------------------------------------------
void loadSettings()
{
  baudrate             = prefs.getInt("baudrate", 115200);
  logo                 = prefs.getInt("logo_configs", 0);
  display_setting      = prefs.getInt("display_configs", 0);
  SCREEN_HEIGHT        = prefs.getInt("screen_height", 64);
  buttons              = prefs.getInt("buttons_configs", 2);
  Separate_or_together = prefs.getInt("SorT_configs", 0);
  nrf_pa               = prefs.getInt("PA_configs", 3);
  bluetooth_jam_method = prefs.getInt("bt_configs", 0);
  drone_jam_method     = prefs.getInt("drone_configs", 0);
  misc_jam_method      = prefs.getInt("misc_configs", 0);
  access_point         = prefs.getInt("AP_configs", 0);
  jam_delay            = prefs.getInt("jam_delay", 0);

  // One-time migration.  The settings page used to default PA to 0 ("Lowest",
  // RF24_PA_MIN = -18 dBm) and delay to 0, and pressing Save once (e.g. to set
  // the nRF24 pins) persisted those values.  Every jammer then ran 30+ dB down
  // forever.  Bump cfg_ver so an intentional "Lowest" choice is preserved
  // from now on.
  const int CFG_VERSION = 2;
  int       cfgVer      = prefs.getInt("cfg_ver", 1);
  if ( cfgVer < CFG_VERSION )
  {
    if ( nrf_pa == 0 )
      nrf_pa = 3;
    prefs.putInt("PA_configs", nrf_pa);
    prefs.putInt("cfg_ver", CFG_VERSION);
    Serial.println("[settings] migrated PA to maximum (was the old default of -18 dBm)");
  }

  // Sanitise anything that could brick the display or silently kill the PA.
  if ( SCREEN_HEIGHT != 32 && SCREEN_HEIGHT != 64 )
    SCREEN_HEIGHT = 64;
  if ( nrf_pa < 0 || nrf_pa > 3 )
    nrf_pa = 3;
  if ( jam_delay < 0 )
    jam_delay = 0;
  if ( jam_delay > 10000 )
    jam_delay = 10000;
  if ( Separate_or_together < 0 || Separate_or_together > 1 )
    Separate_or_together = 0;
  if ( access_point < 0 || access_point > 1 )
    access_point = 0;
  prefs.getBytes("SSIDs_Array", SSIDs_Array, sizeof(SSIDs_Array));

  // Joystick configuration
  joy_inverted  = prefs.getInt("joy_inv", 1);
  joy_hold_ms   = prefs.getInt("joy_hold", 350);
  joy_repeat_ms = prefs.getInt("joy_rep", 140);
  joy_deadzone  = prefs.getInt("joy_dead", 800);

  if ( joy_inverted < 0 || joy_inverted > 1 ) joy_inverted = 1;
  if ( joy_hold_ms < 100 || joy_hold_ms > 2000 ) joy_hold_ms = 350;
  if ( joy_repeat_ms < 50 || joy_repeat_ms > 1000 ) joy_repeat_ms = 140;
  if ( joy_deadzone < 200 || joy_deadzone > 1800 ) joy_deadzone = 800;

  Serial.printf("[settings] PA=%d delay=%d height=%d sweep=%d AP=%d modules=%d\n",
                nrf_pa, jam_delay, SCREEN_HEIGHT, Separate_or_together, access_point, nrf24_count);
}

void handlenRF24Init()
{
  nrf24_count = prefs.getInt("nRF24_count", 0);

  prefs.getBytes("nRF24_ce_pins", ce_pins, sizeof(ce_pins));
  prefs.getBytes("nRF24_csn_pins", csn_pins, sizeof(csn_pins));
}

void handlenRF24Scan()
{
  int  found[30], count[30];
  int  n = detectNRF24Modules(found, count);

  if ( n > 0 )
  {
    for ( int i = 0; i < n; i++ )
    {
      ce_pins[i]  = found[i];
      csn_pins[i] = count[i];
    }
    nrf24_count = n;

    prefs.putBytes("nRF24_ce_pins", ce_pins, sizeof(ce_pins));
    prefs.putBytes("nRF24_csn_pins", csn_pins, sizeof(csn_pins));
    prefs.putInt("nRF24_count", nrf24_count);
  }
}

static bool isSafePin(int pin)
{
  if ( !GPIO_IS_VALID_OUTPUT_GPIO(pin) )
    return false;
  switch ( pin )
  {
  case 0:
  case 1:
  case 3:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
  case 22:
  case 21:
  case 25:
  case 26:
  case 27:
  case 2:
    return false;
  default:
    return true;
  }
}

// Detect physically-present nRF24 modules on the shared HSPI bus.
// Returns number of modules found (0-30).
//   found[] : CE pins in detected slots
//   count[] : CSN pins in detected slots
// Pair (found[i], count[i]) is one module.
int detectNRF24Modules(int *found_ce, int *found_csn)
{
  HSPI_initBare();
  auto readReg = [&](int csnPin, uint8_t reg) {
    for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
    {
      if ( isSafePin(p) && p != csnPin )
        digitalWrite(p, HIGH);
    }
    hp->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(csnPin, LOW);
    hp->transfer(0x00 | reg);
    uint8_t val = hp->transfer(0xFF);
    digitalWrite(csnPin, HIGH);
    hp->endTransaction();
    return val;
  };
  auto writeReg = [&](int csnPin, uint8_t reg, uint8_t val) {
    for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
    {
      if ( isSafePin(p) && p != csnPin )
        digitalWrite(p, HIGH);
    }
    hp->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(csnPin, LOW);
    hp->transfer(0x20 | reg);
    hp->transfer(val);
    digitalWrite(csnPin, HIGH);
    hp->endTransaction();
  };

  for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
  {
    if ( isSafePin(p) )
    {
      pinMode(p, OUTPUT);
      digitalWrite(p, HIGH);
    }
  }

  int csn_list[30];
  int csn_count = 0;
  for ( int csn = 0; csn < SOC_GPIO_PIN_COUNT; csn++ )
  {
    if ( !isSafePin(csn) )
      continue;

    yield();

    writeReg(csn, 0x05, 0x33);
    uint8_t test = readReg(csn, 0x05);
    if ( test != 0x33 )
      continue;

    writeReg(csn, 0x05, 0x44);
    test = readReg(csn, 0x05);
    if ( test != 0x44 )
      continue;

    csn_list[csn_count++] = csn;
  }

  for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
  {
    if ( !isSafePin(p) )
      continue;
    bool is_csn = false;
    for ( int i = 0; i < csn_count; i++ )
    {
      if ( csn_list[i] == p )
      {
        is_csn = true;
        break;
      }
    }
    if ( !is_csn )
      digitalWrite(p, LOW);
  }

  if ( csn_count == 0 )
  {
    for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
    {
      if ( isSafePin(p) )
        pinMode(p, INPUT);
    }
    HSPI_deinit();
    return 0;
  }

  int  ce_list[30];
  int  csn_of[30]; // the CSN each detected CE was actually proven against
  int  ce_count = 0;
  bool csn_done[30] = {false};

  for ( int ce = 0; ce < SOC_GPIO_PIN_COUNT; ce++ )
  {
    if ( !isSafePin(ce) )
      continue;

    yield();

    auto csn = std::find(csn_list, csn_list + csn_count, ce);
    if ( csn != csn_list + csn_count )
      continue;

    for ( int i = 0; i < csn_count; i++ )
    {
      if ( csn_done[i] )
        continue;

      int csn_pin = csn_list[i];

      writeReg(csn_pin, 0x01, 0x00);
      writeReg(csn_pin, 0x00, 0x0E);
      delay(5);

      writeReg(csn_pin, 0x07, 0x70);

      // Hold every other candidate CE LOW before pulsing this one.
      //
      // The previous code drove all other safe pins HIGH here.  That silently
      // raised the REAL CE lines, so the module transmitted on its own, TX_DS
      // was set no matter which pin was pulsed, and *any* free GPIO was
      // accepted as "the" CE.  That is exactly how unconnected pins 4 and 5
      // were reported as CE for modules 0 and 1.
      for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
      {
        if ( !isSafePin(p) || p == csn_pin )
          continue;
        bool is_csn_pin = false;
        for ( int k = 0; k < csn_count; k++ )
        {
          if ( csn_list[k] == p )
          {
            is_csn_pin = true;
            break;
          }
        }
        if ( !is_csn_pin )
          digitalWrite(p, LOW);
      }
      hp->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
      digitalWrite(csn_pin, LOW);
      hp->transfer(0xE1);
      digitalWrite(csn_pin, HIGH);

      digitalWrite(csn_pin, LOW);
      hp->transfer(0xA0);
      hp->transfer(0x55);
      digitalWrite(csn_pin, HIGH);
      hp->endTransaction();

      digitalWrite(ce, HIGH);
      delayMicroseconds(50);
      digitalWrite(ce, LOW);

      delay(5);

      // Read STATUS with the other CE lines still held low, otherwise the
      // module can start a second transmission during the read.
      uint8_t status = 0;
      for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
      {
        if ( !isSafePin(p) || p == csn_pin )
          continue;
        bool is_csn_pin = false;
        for ( int k = 0; k < csn_count; k++ )
        {
          if ( csn_list[k] == p )
          {
            is_csn_pin = true;
            break;
          }
        }
        if ( !is_csn_pin )
          digitalWrite(p, LOW);
      }
      hp->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
      digitalWrite(csn_pin, LOW);
      hp->transfer(0x07);
      status = hp->transfer(0xFF);
      digitalWrite(csn_pin, HIGH);
      hp->endTransaction();

      if ( (status & 0x20) == 0 )
        continue;

      ce_list[ce_count] = ce;
      csn_of[ce_count]  = csn_pin;
      ce_count++;
      csn_done[i] = true;
      break;
    }
  }

  HSPI_deinit();

  int n = (ce_count < csn_count) ? ce_count : csn_count;
  for ( int i = 0; i < n; i++ )
  {
    // Pair each CE with the CSN it was proven against.  Pairing by index (the
    // old behaviour) shuffled CE/CSN whenever the two scans finished in a
    // different order.
    found_ce[i]  = ce_list[i];
    found_csn[i] = csn_of[i];
  }

  // Restore non-detected probed pins to INPUT to avoid line contention
  for ( int p = 0; p < SOC_GPIO_PIN_COUNT; p++ )
  {
    if ( !isSafePin(p) )
      continue;
    bool is_module_pin = false;
    for ( int i = 0; i < n; i++ )
    {
      if ( found_csn[i] == p || found_ce[i] == p )
      {
        is_module_pin = true;
        break;
      }
    }
    if ( !is_module_pin )
    {
      pinMode(p, INPUT);
    }
  }

  // Set detected module pins to idle/unselected state
  for ( int i = 0; i < n; i++ )
  {
    pinMode(found_csn[i], OUTPUT);
    digitalWrite(found_csn[i], HIGH);
    pinMode(found_ce[i], OUTPUT);
    digitalWrite(found_ce[i], LOW);
  }

  return n;
}

// Check whether a configured module (ce, csn) is alive and responding.
// Returns true if SPI round-trip on config register 0x05 succeeds.
// Both core Task WDTs are disabled at startup (see setup()), so the
// blocking SPI round-trip here will not trip the watchdog.
bool checkNRF24Module(int ce, int csn)
{
  HSPI_initBare();

  for ( int i = 0; i < nrf24_count; i++ )
  {
    if ( csn_pins[i] != csn && isSafePin(csn_pins[i]) )
    {
      pinMode(csn_pins[i], OUTPUT);
      digitalWrite(csn_pins[i], HIGH);
    }
  }
  pinMode(csn, OUTPUT);
  digitalWrite(csn, HIGH);

  auto poke = [&](uint8_t reg, uint8_t val) {
    hp->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(csn, LOW);
    hp->transfer(0x20 | reg);
    hp->transfer(val);
    digitalWrite(csn, HIGH);
    hp->endTransaction();
  };
  auto probe = [&](uint8_t reg) -> uint8_t {
    hp->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    digitalWrite(csn, LOW);
    hp->transfer(0x00 | reg);
    uint8_t v = hp->transfer(0xFF);
    digitalWrite(csn, HIGH);
    hp->endTransaction();
    return v;
  };

  bool ok = false;
  if ( csn >= 0 && csn < SOC_GPIO_PIN_COUNT )
  {
    poke(0x05, 0x33);
    delay(1);
    uint8_t v1 = probe(0x05);
    delay(1);
    poke(0x05, 0x44);
    delay(1);
    uint8_t v2 = probe(0x05);
    delay(1);
    yield();
    delay(1);
    ok = (v1 == 0x33 && v2 == 0x44);
  }

  HSPI_deinit();

  return ok;
}
void RescanHandler()
{
  scan_wifi_APs(WiFiScanChannels, false);
  handleRoot();
}

void sendHtmlAndExecute(const uint8_t *htmlData, size_t len, void (*action)() = nullptr)
{
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)htmlData, len);
  delay(500);
  if ( action )
    action();
}

void attackHandler(String htmlResponse, void (*attackFunction)(), const unsigned char *bitmap)
{
  (void)htmlResponse;
  if ( bitmap != nullptr )
  {
    display.clearDisplay();
    display.drawBitmap(0, 0, bitmap, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
    display.display();
  }
  sendHtmlAndExecute(html_jam, html_jam_len, attackFunction);
  updateDisplay(menu_number);
}

void attackScanHandler(String htmlResponse, void (*attackFunction)(), const unsigned char *bitmap)
{
  (void)htmlResponse;
  if ( bitmap != nullptr )
  {
    display.clearDisplay();
    display.drawBitmap(0, 0, bitmap, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
    display.display();
  }
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)html_jam, html_jam_len);
  delay(500);
  if ( access_point == 0 )
    WiFi.softAPdisconnect();
  WiFi.mode(WIFI_STA);
  delay(100);
  NumberChannels = scan_wifi_APs(WiFiScanChannels, true);
  attackFunction();
  updateDisplay(menu_number);
}

void miscChannelsHandler()
{
  int channel1 = server.arg("start").toInt();
  int channel2 = server.arg("stop").toInt();

  sendHtmlAndExecute(html_jam, html_jam_len);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Start");
  display.setCursor(96, 0);
  display.println("Stop");
  display.setCursor(52, 0);
  display.println("Width");
  display.setCursor(8, 10);
  display.println(String(channel1));
  display.setCursor(60, 10);
  display.println(String(channel2 - channel1));
  display.setCursor(100, 10);
  display.println(String(channel2));
  display.setCursor(0, 20);
  display.println("Jamming Started");
  display.display();

  jamHandler(misc_jam, "Jamming from " + String(channel1) + " to " + String(channel2), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_misc_jammer_64 : bitmap_misc_jammer_32), false, false, true, false, channel1, channel2);
  updateDisplay(menu_number);
}

void wifiChannelsHandler()
{
  int channel = server.arg("channel").toInt();

  display.clearDisplay();
  display.drawBitmap(0, 0, decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_wifi_jam_64 : bitmap_wifi_jam_32), SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  display.display();
  sendHtmlAndExecute(html_jam, html_jam_len);

  jamHandler(wifi_channel, "Jamming WiFi channel", decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_wifi_jam_64 : bitmap_wifi_jam_32), false, false, false, false, channel - 1, 0);
  updateDisplay(menu_number);
}

void wifiDeauthChannelsHandler()
{
  int channel = server.arg("channel").toInt();

  display.clearDisplay();
  display.drawBitmap(0, 0, decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_wifi_deauth_64 : bitmap_wifi_deauth_32), SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  display.display();
  sendHtmlAndExecute(html_jam, html_jam_len);

  wifi_deauth_channel(channel);
  updateDisplay(menu_number);
}

void handleApiStatus()
{
  String json;
  json.reserve(1024);

  json += "{\"nrf24_count\":" + String(nrf24_count) + ",";

  String btText;
  switch ( bluetooth_jam_method )
  {
  case 0:
    btText = "21 channels";
    break;
  case 1:
  case 2:
    btText = "80 channels";
    break;
  default:
    btText = "-";
  }
  json += "\"bt_channels_text\":\"" + btText + "\",";
  json += "\"wifi_jam_method\":" + String(wifi_jam_method) + ",";

  json += "\"ssids_by_channel\":[";
  for ( int ch = 0; ch < 14; ch++ )
  {
    json += "\"" + jsonEscape(String(APs_array[ch])) + "\"";
    if ( ch < 13 )
      json += ",";
  }
  json += "],";

  json += "\"wifi_channels\":[";
  for ( int ch = 0; ch < 14; ch++ )
  {
    json += String(WiFiScanChannels[ch]);
    if ( ch < 13 )
      json += ",";
  }
  json += "]";

  json += "}";
  server.send(200, "application/json", json);
}

void handleApiSettings()
{
  String json;
  json.reserve(2048);

  json += "{\"version\":\"" + String(Version_Number) + "\",";
  json += "\"baudrate\":" + String(baudrate) + ",";
  json += "\"logo\":" + String(logo) + ",";
  json += "\"display\":" + String(display_setting) + ",";
  json += "\"screen_height\":" + String(SCREEN_HEIGHT) + ",";
  json += "\"button\":" + String(buttons) + ",";
  json += "\"sweep\":" + String(Separate_or_together) + ",";
  json += "\"pa\":" + String(nrf_pa) + ",";
  json += "\"bt\":" + String(bluetooth_jam_method) + ",";
  json += "\"drone\":" + String(drone_jam_method) + ",";
  json += "\"misc\":" + String(misc_jam_method) + ",";
  json += "\"delay\":" + String(jam_delay) + ",";
  json += "\"ap\":" + String(access_point) + ",";
  json += "\"nrf24_count\":" + String(nrf24_count) + ",";

  json += "\"ssid\":\"" + jsonEscape(ssid) + "\",";
  json += "\"password\":\"" + jsonEscape(password) + "\",";

  json += "\"beacon\":[";
  bool firstB = true;
  for ( int i = 0; i < 100; i++ )
  {
    if ( SSIDs_Array[i][0] == '\0' )
      break;
    if ( !firstB )
      json += ",";
    json += "\"" + jsonEscape(String(SSIDs_Array[i])) + "\"";
    firstB = false;
  }
  json += "],";

  json += "\"ce_pins\":[";
  for ( int i = 0; i < nrf24_count; i++ )
  {
    json += String(ce_pins[i]);
    if ( i < nrf24_count - 1 )
      json += ",";
  }
  json += "],";

  json += "\"csn_pins\":[";
  for ( int i = 0; i < nrf24_count; i++ )
  {
    json += String(csn_pins[i]);
    if ( i < nrf24_count - 1 )
      json += ",";
  }
  json += "]";

  json += "}";
  server.send(200, "application/json", json);
}

void handleApiVersion()
{
  String json = "{\"version\":\"" + String(Version_Number) + "\"}";
  server.send(200, "application/json", json);
}

void storeEEPROMAndSet(const char *index, int value, int &targetVar)
{
  prefs.putInt(index, value);
  targetVar = value;
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void saveWiFiSettings(const char *new_ssid, const char *new_password)
{
  prefs.putString("ssid", new_ssid);

  prefs.putString("password", new_password);
}

void handleResetWiFiSettings()
{
  saveWiFiSettings(default_ssid, default_password);

  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char *)html_pls_reboot, html_pls_reboot_len);
  delay(1000);
  ESP.restart();
}

void handleFileUpload()
{
  HTTPUpload          &upload         = server.upload();
  static unsigned long lastUpdate     = 0;
  const unsigned long  updateInterval = 10;

  if ( upload.status == UPLOAD_FILE_START )
  {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if ( !Update.begin(UPDATE_SIZE_UNKNOWN) )
    {
      Update.printError(Serial);
      return;
    }
    else
    {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      lastUpdate = millis();
    }
  }
  else if ( upload.status == UPLOAD_FILE_WRITE )
  {
    if ( Update.write(upload.buf, upload.currentSize) != upload.currentSize )
    {
      Update.printError(Serial);
    }

    if ( millis() - lastUpdate >= updateInterval )
    {
      lastUpdate = millis();
    }
  }
  else if ( upload.status == UPLOAD_FILE_END )
  {
  }
  else if ( upload.status == UPLOAD_FILE_ABORTED )
  {
    Update.abort();
    Serial.println("Update aborted");
  }
}

// Live nRF24 detection status screen. Runs the hardware scan, shows each
// Live nRF24 detection status screen. Styled as a unified diagnostic dashboard.
void nrfStatusScreen()
{
  int  found_ce[30], found_csn[30];
  int  found = detectNRF24Modules(found_ce, found_csn);

  int scrollOffset = 0;
  const int maxVisible = 4; // Comfortably fits 4 modules on screen without overlapping hint!

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("nRF24 Diagnostics");

    // Diagnostic summary banner
    display.fillRect(2, 13, SCREEN_WIDTH - 4, 9, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(4, 14);
    display.print("Detected: ");
    display.print(found);
    display.print("/");
    display.print(nrf24_count);
    display.print(" ");
    if ( found >= nrf24_count && nrf24_count > 0 )
      display.print("[ALL OK]");
    else if ( found == 0 )
      display.print("[NO NRF]");
    else
      display.print("[PARTIAL]");

    int totalItems = (found > nrf24_count) ? found : nrf24_count;
    if ( totalItems == 0 )
    {
      display.setTextColor(WHITE, BLACK);
      display.setCursor(8, 28);
      display.print("No nRF24 modules detected");
      display.setCursor(8, 38);
      display.print("Check SPI wiring & 3.3V");
    }
    else
    {
      for ( int i = 0; i < maxVisible; i++ )
      {
        int idx = scrollOffset + i;
        if ( idx >= totalItems ) break;

        int y = 24 + i * 8;
        bool isDetected = false;
        int ce = 0, csn = 0;

        if ( idx < found )
        {
          ce = found_ce[idx];
          csn = found_csn[idx];
          isDetected = true;
        }
        else if ( idx < nrf24_count )
        {
          ce = ce_pins[idx];
          csn = csn_pins[idx];
          isDetected = false;
        }

        display.drawRoundRect(2, y - 1, 124, 8, 2, WHITE);
        display.setTextColor(WHITE, BLACK);
        display.setCursor(4, y);
        display.print("M");
        display.print(idx);
        display.print(" CE:");
        display.print(ce);
        if ( ce < 10 ) display.print(" ");
        display.print(" CSN:");
        display.print(csn);
        if ( csn < 10 ) display.print(" ");

        if ( isDetected )
        {
          display.fillRect(100, y - 1, 25, 8, WHITE);
          display.setTextColor(BLACK, WHITE);
          display.setCursor(102, y);
          display.print(" OK ");
        }
        else
        {
          display.setTextColor(WHITE, BLACK);
          display.setCursor(102, y);
          display.print("MISS");
        }
      }
    }

    // Scroll arrows if more items exist
    if ( scrollOffset > 0 )
    {
      display.setTextColor(WHITE, BLACK);
      display.setCursor(120, 24);
      display.print("^");
    }
    if ( scrollOffset + maxVisible < totalItems )
    {
      display.setTextColor(WHITE, BLACK);
      display.setCursor(120, 48);
      display.print("v");
    }

    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();

    int totalItems = (found > nrf24_count) ? found : nrf24_count;
    if ( btnNext.isSingle() && scrollOffset + maxVisible < totalItems )
    {
      scrollOffset++;
      draw();
    }
    if ( btnPrevious.isSingle() && scrollOffset > 0 )
    {
      scrollOffset--;
      draw();
    }
    if ( btnOK.isSingle() || btnOK.isHolded() )
    {
      break;
    }
    delay(1);
  }
}

void joystickConfigScreen()
{
  static const char *const labels[] = {
      "Invert Axis",
      "Hold Delay",
      "Repeat Rate",
      "Deadzone",
      "Live Monitor",
      "< Back"
  };
  const int itemCount = 6;
  int current = 0;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("Joystick Settings");

    const int firstY = 14;
    const int rowH = 9;
    const int maxVisible = (SCREEN_HEIGHT == 64) ? 4 : 2;
    int scroll = current - (maxVisible / 2);
    if ( scroll < 0 ) scroll = 0;
    if ( scroll + maxVisible > itemCount ) scroll = itemCount - maxVisible;

    display.setTextSize(1);
    for ( int i = 0; i < maxVisible; i++ )
    {
      int idx = scroll + i;
      if ( idx >= itemCount ) break;
      int y = firstY + i * rowH;
      bool sel = (idx == current);
      if ( sel )
      {
        display.fillRect(0, y - 1, SCREEN_WIDTH, rowH, WHITE);
        display.setTextColor(BLACK, WHITE);
      }
      else
        display.setTextColor(WHITE, BLACK);

      display.setCursor(4, y);
      display.print(labels[idx]);

      display.setCursor(78, y);
      switch ( idx )
      {
      case 0:
        display.print(joy_inverted ? "[YES]" : "[NO]");
        break;
      case 1:
        display.print(joy_hold_ms);
        display.print("ms");
        break;
      case 2:
        display.print(joy_repeat_ms);
        display.print("ms");
        break;
      case 3:
        display.print(joy_deadzone);
        break;
      case 4:
        display.print(">>>");
        break;
      case 5:
        display.print("");
        break;
      }
    }
    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();

    if ( btnNext.isSingle() )
    {
      current = (current + 1) % itemCount;
      draw();
    }
    if ( btnPrevious.isSingle() )
    {
      current = (current - 1 + itemCount) % itemCount;
      draw();
    }
    if ( btnOK.isHolded() )
    {
      break;
    }
    if ( btnOK.isSingle() )
    {
      switch ( current )
      {
      case 0: // Invert
        joy_inverted = (joy_inverted == 1) ? 0 : 1;
        prefs.putInt("joy_inv", joy_inverted);
        break;
      case 1: // Hold Delay
      {
        static const int holdSteps[] = {150, 250, 350, 500, 750};
        int hIdx = 2;
        for ( int s = 0; s < 5; s++ )
          if ( holdSteps[s] == joy_hold_ms ) hIdx = s;
        hIdx = (hIdx + 1) % 5;
        joy_hold_ms = holdSteps[hIdx];
        prefs.putInt("joy_hold", joy_hold_ms);
        break;
      }
      case 2: // Repeat Rate
      {
        static const int repSteps[] = {80, 100, 140, 200, 300};
        int rIdx = 2;
        for ( int s = 0; s < 5; s++ )
          if ( repSteps[s] == joy_repeat_ms ) rIdx = s;
        rIdx = (rIdx + 1) % 5;
        joy_repeat_ms = repSteps[rIdx];
        prefs.putInt("joy_rep", joy_repeat_ms);
        break;
      }
      case 3: // Deadzone
      {
        static const int deadSteps[] = {400, 600, 800, 1000, 1200};
        int dIdx = 2;
        for ( int s = 0; s < 5; s++ )
          if ( deadSteps[s] == joy_deadzone ) dIdx = s;
        dIdx = (dIdx + 1) % 5;
        joy_deadzone = deadSteps[dIdx];
        prefs.putInt("joy_dead", joy_deadzone);
        break;
      }
      case 4: // Live Monitor
      {
        while ( true )
        {
          updateJoystickButtons();
          display.clearDisplay();
          drawMenuFrame("Joystick Live Test");

          int rawX = analogRead(JOY_VRX);
          int rawY = analogRead(JOY_VRY);
          int swVal = digitalRead(JOY_SW);

          display.setTextColor(WHITE, BLACK);
          display.setCursor(4, 16);
          display.print("VRx: "); display.print(rawX);
          display.setCursor(4, 26);
          display.print("VRy: "); display.print(rawY);
          display.setCursor(4, 36);
          display.print("SW : "); display.print(swVal == LOW ? "PRESSED" : "UP");

          // Crosshair visual
          int cx = 100, cy = 30;
          display.drawCircle(cx, cy, 14, WHITE);
          display.drawFastHLine(cx - 16, cy, 32, WHITE);
          display.drawFastVLine(cx, cy - 16, 32, WHITE);
          int dotX = map(rawX, 0, 4095, cx - 11, cx + 11);
          int dotY = map(rawY, 0, 4095, cy - 11, cy + 11);
          display.fillCircle(dotX, dotY, 2, WHITE);

          display.setCursor(4, 46);
          display.print("Act: ");
          if ( btnNext.state() ) display.print("NEXT >>");
          else if ( btnPrevious.state() ) display.print("<< PREV");
          else display.print("CENTER");

          display.drawFastHLine(0, 55, SCREEN_WIDTH, WHITE);
          display.setCursor(4, 57);
          display.print("Click SW to exit");
          display.display();

          if ( btnOK.isSingle() || btnOK.isHolded() )
          {
            break;
          }
          delay(20);
        }
        break;
      }
      case 5: // Back
        return;
      }
      draw();
    }
    delay(1);
  }
}

void settings()
{
  int  current_item  = 0;
  int  scroll_offset = 0;
  bool in_edit_mode  = false;
  const int total_items = 15;
  const int maxVisible = (SCREEN_HEIGHT == 64) ? 4 : 2;
  const int rowH = 9;
  const int firstY = 14;

  auto draw_settings = [&]() {
    if ( current_item >= total_items )
      current_item = total_items - 1;

    display.clearDisplay();
    drawMenuFrame("Settings");

    if ( current_item < scroll_offset )
      scroll_offset = current_item;
    else if ( current_item >= scroll_offset + maxVisible )
      scroll_offset = current_item - maxVisible + 1;

    for ( int i = 0; i < maxVisible; i++ )
    {
      int item_idx = scroll_offset + i;
      if ( item_idx >= total_items )
        break;

      int y = firstY + i * rowH;
      bool sel = (item_idx == current_item);

      if ( sel )
      {
        display.fillRect(0, y - 1, SCREEN_WIDTH, rowH, WHITE);
        display.setTextColor(BLACK, WHITE);
      }
      else
      {
        display.setTextColor(WHITE, BLACK);
      }

      display.setCursor(4, y);

      auto printVal = [&](const String &label, const String &val, int item_idx) {
        display.print(label);
        if ( in_edit_mode && current_item == item_idx )
        {
          display.print("< ");
          display.print(val);
          display.print(" >");
        }
        else
        {
          display.print(val);
        }
      };

      switch ( item_idx )
      {
      case 0:
        printVal("Logo: ", logo == 0 ? "Show" : "Hide", 0);
        break;
      case 1:
        printVal("Display: ", display_setting == 0 ? "On" : "Off", 1);
        break;
      case 2:
        printVal("Height: ", String(SCREEN_HEIGHT), 2);
        break;
      case 3:
        printVal("Buttons: ", buttons == 0 ? "1 Btn" : (buttons == 1 ? "2 Btn" : "3-way Joy"), 3);
        break;
      case 4:
        printVal("Sweep: ", Separate_or_together == 0 ? "Separate" : "Together", 4);
        break;
      case 5:
        printVal("PA Level: ", nrf_pa == 0 ? "Lowest" : (nrf_pa == 1 ? "Low" : (nrf_pa == 2 ? "High" : "Ultra")), 5);
        break;
      case 6:
        printVal("BT Mode: ", bluetooth_jam_method == 0 ? "21ch" : (bluetooth_jam_method == 1 ? "80ch rand" : "80ch brute"), 6);
        break;
      case 7:
        printVal("Drone: ", drone_jam_method == 0 ? "Random" : "Brute", 7);
        break;
      case 8:
        printVal("Misc: ", misc_jam_method == 0 ? "CW" : (misc_jam_method == 1 ? "Packet" : (misc_jam_method == 2 ? "Full 2.4G" : (misc_jam_method == 3 ? "USB" : "FPV"))), 8);
        break;
      case 9:
        printVal("AP Mode: ", access_point == 0 ? "On" : "Off", 9);
        break;
      case 10:
        printVal("Baudrate: ", String(baudrate), 10);
        break;
      case 11:
        printVal("Jam Delay: ", String(jam_delay) + "ms", 11);
        break;
      case 12:
        display.print("nRF24: ");
        display.print(nrf24_count);
        display.print(" (diag)");
        break;
      case 13:
        display.print("Joystick: Config >");
        break;
      case 14:
        display.print(" [ Save & Back ]");
        break;
      }
    }

    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(4, 55);
    if ( in_edit_mode )
      display.print("< > Choose | OK: Apply");
    else
      display.print("< > Move | OK: Edit");

    display.display();
  };

  auto apply_and_save_item = [&](int item_idx, int delta) {
    switch ( item_idx )
    {
    case 0:
      logo = (logo + delta + 2) % 2;
      prefs.putInt("logo_configs", logo);
      break;
    case 1:
      display_setting = (display_setting + delta + 2) % 2;
      prefs.putInt("display_configs", display_setting);
      if ( display_setting )
        display.ssd1306_command(SSD1306_DISPLAYOFF);
      else
        display.ssd1306_command(SSD1306_DISPLAYON);
      break;
    case 2:
      SCREEN_HEIGHT = (SCREEN_HEIGHT == 64) ? 32 : 64;
      prefs.putInt("screen_height", SCREEN_HEIGHT);
      if ( SCREEN_HEIGHT == 32 )
      {
        display.ssd1306_command(SSD1306_SETCOMPINS);
        display.ssd1306_command(0x02);
        display.ssd1306_command(SSD1306_SETMULTIPLEX);
        display.ssd1306_command(0x1F);
      }
      else
      {
        display.ssd1306_command(SSD1306_SETCOMPINS);
        display.ssd1306_command(0x12);
        display.ssd1306_command(SSD1306_SETMULTIPLEX);
        display.ssd1306_command(0x3F);
      }
      break;
    case 3:
      buttons = (buttons + delta + 3) % 3;
      prefs.putInt("buttons_configs", buttons);
      break;
    case 4:
      Separate_or_together = (Separate_or_together + delta + 2) % 2;
      prefs.putInt("SorT_configs", Separate_or_together);
      break;
    case 5:
      nrf_pa = (nrf_pa + delta + 4) % 4;
      prefs.putInt("PA_configs", nrf_pa);
      break;
    case 6:
      bluetooth_jam_method = (bluetooth_jam_method + delta + 3) % 3;
      prefs.putInt("bt_configs", bluetooth_jam_method);
      break;
    case 7:
      drone_jam_method = (drone_jam_method + delta + 2) % 2;
      prefs.putInt("drone_configs", drone_jam_method);
      break;
    case 8:
      misc_jam_method = (misc_jam_method + delta + 5) % 5;
      prefs.putInt("misc_configs", misc_jam_method);
      break;
    case 9:
      access_point = (access_point + delta + 2) % 2;
      prefs.putInt("AP_configs", access_point);
      break;
    case 10: {
      const uint32_t baudrate_list[] = {9600, 19200, 38400, 57600, 115200, 921600};
      const int      baudrate_count  = sizeof(baudrate_list) / sizeof(baudrate_list[0]);
      int            idx             = 0;
      for ( int i = 0; i < baudrate_count; i++ )
      {
        if ( baudrate_list[i] == (uint32_t)baudrate )
        {
          idx = i;
          break;
        }
      }
      idx      = (idx + delta + baudrate_count) % baudrate_count;
      baudrate = baudrate_list[idx];
      prefs.putInt("baudrate", baudrate);
      Serial.updateBaudRate(baudrate);
      break;
    }
    case 11:
      jam_delay += delta * 10;
      if ( jam_delay > 10000 )
        jam_delay = 0;
      if ( jam_delay < 0 )
        jam_delay = 10000;
      prefs.putInt("jam_delay", jam_delay);
      break;
    case 12:
      nrfStatusScreen();
      break;
    case 13:
      joystickConfigScreen();
      break;
    case 14:
      break;
    }
  };

  draw_settings();

  while ( true )
  {
    updateJoystickButtons();

    if ( in_edit_mode )
    {
      if ( btnNext.isSingle() || btnNext.isHold() )
      {
        if ( btnNext.isHold() )
          delay(100);
        apply_and_save_item(current_item, 1);
        draw_settings();
      }
      else if ( btnPrevious.isSingle() || btnPrevious.isHold() )
      {
        if ( btnPrevious.isHold() )
          delay(100);
        apply_and_save_item(current_item, -1);
        draw_settings();
      }
      else if ( btnOK.isSingle() || btnOK.isHolded() )
      {
        in_edit_mode = false;
        draw_settings();
      }
    }
    else
    {
      if ( btnNext.isSingle() || btnNext.isHold() )
      {
        if ( btnNext.isHold() )
          delay(100);
        current_item = (current_item + 1) % total_items;
        draw_settings();
      }
      else if ( btnPrevious.isSingle() || btnPrevious.isHold() )
      {
        if ( btnPrevious.isHold() )
          delay(100);
        current_item = (current_item - 1 + total_items) % total_items;
        draw_settings();
      }
      else if ( btnOK.isSingle() )
      {
        if ( current_item == 14 )
        {
          break;
        }
        else if ( current_item == 12 )
        {
          nrfStatusScreen();
          draw_settings();
        }
        else if ( current_item == 13 )
        {
          joystickConfigScreen();
          draw_settings();
        }
        else
        {
          in_edit_mode = true;
          draw_settings();
        }
      }
      else if ( btnOK.isHolded() )
      {
        break;
      }
    }
    delay(1);
  }

  display.setTextColor(WHITE, BLACK);
  updateDisplay(menu_number);
}

// ---------------------------------------------------------------------------
// Misc Jammer (Frequency Sweep) Screen & Submenu — Unified Layout
// ---------------------------------------------------------------------------
void misc_custom_sweep()
{
  int  selectedField = 0; // 0 = Start Ch, 1 = Stop Ch, 2 = Start Jamming
  bool editMode      = false;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("Custom Sweep");

    // Start Channel Card
    int y0 = 14;
    if ( selectedField == 0 )
    {
      display.fillRect(2, y0, SCREEN_WIDTH - 4, 10, WHITE);
      display.setTextColor(BLACK, WHITE);
    }
    else
    {
      display.drawRoundRect(2, y0, SCREEN_WIDTH - 4, 10, 2, WHITE);
      display.setTextColor(WHITE, BLACK);
    }
    display.setCursor(6, y0 + 1);
    display.print("Start: ");
    if ( editMode && selectedField == 0 ) display.print("< ");
    display.print("Ch ");
    if ( channel1 < 10 ) display.print(" ");
    display.print(channel1);
    display.print(" (");
    display.print(2400 + channel1);
    display.print("M)");
    if ( editMode && selectedField == 0 ) display.print(" >");

    // Stop Channel Card
    int y1 = 26;
    if ( selectedField == 1 )
    {
      display.fillRect(2, y1, SCREEN_WIDTH - 4, 10, WHITE);
      display.setTextColor(BLACK, WHITE);
    }
    else
    {
      display.drawRoundRect(2, y1, SCREEN_WIDTH - 4, 10, 2, WHITE);
      display.setTextColor(WHITE, BLACK);
    }
    display.setCursor(6, y1 + 1);
    display.print("Stop : ");
    if ( editMode && selectedField == 1 ) display.print("< ");
    display.print("Ch ");
    if ( channel2 < 10 ) display.print(" ");
    display.print(channel2);
    display.print(" (");
    display.print(2400 + channel2);
    display.print("M)");
    if ( editMode && selectedField == 1 ) display.print(" >");

    // Bandwidth Span line
    display.setTextColor(WHITE, BLACK);
    display.setCursor(6, 38);
    display.print("Span : ");
    int span = channel2 - channel1;
    if ( span >= 0 )
    {
      display.print(span + 1);
      display.print(" MHz (");
      display.print(span + 1);
      display.print(" chs)");
    }
    else
    {
      display.print("ERR: Stop < Start");
    }

    // Action button
    int y2 = 47;
    if ( selectedField == 2 )
    {
      display.fillRect(2, y2, SCREEN_WIDTH - 4, 10, WHITE);
      display.setTextColor(BLACK, WHITE);
    }
    else
    {
      display.drawRoundRect(2, y2, SCREEN_WIDTH - 4, 10, 2, WHITE);
      display.setTextColor(WHITE, BLACK);
    }
    display.setCursor(12, y2 + 1);
    display.print("[ START SWEEP JAM ]");

    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(4, 57);
    if ( editMode )
      display.print("< > Value | OK: Apply");
    else
      display.print("< > Move | OK: Select");

    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();

    if ( editMode )
    {
      int &targetCh = (selectedField == 0) ? channel1 : channel2;
      if ( btnNext.isSingle() || btnNext.isHold() )
      {
        targetCh++;
        if ( targetCh > 125 ) targetCh = 0;
        draw();
        if ( btnNext.isHold() ) delay(30);
      }
      if ( btnPrevious.isSingle() || btnPrevious.isHold() )
      {
        targetCh--;
        if ( targetCh < 0 ) targetCh = 125;
        draw();
        if ( btnPrevious.isHold() ) delay(30);
      }
      if ( btnOK.isSingle() || btnOK.isHolded() )
      {
        editMode = false;
        draw();
      }
    }
    else
    {
      if ( btnNext.isSingle() )
      {
        selectedField = (selectedField + 1) % 3;
        draw();
      }
      if ( btnPrevious.isSingle() )
      {
        selectedField = (selectedField - 1 + 3) % 3;
        draw();
      }
      if ( btnOK.isHolded() )
      {
        return;
      }
      if ( btnOK.isSingle() )
      {
        if ( selectedField == 0 || selectedField == 1 )
        {
          editMode = true;
          draw();
        }
        else if ( selectedField == 2 )
        {
          if ( channel1 > channel2 )
          {
            display.clearDisplay();
            drawMenuFrame("Custom Sweep");
            display.setTextColor(WHITE, BLACK);
            display.setCursor(10, 26);
            display.print("ERROR: Stop < Start!");
            display.setCursor(10, 38);
            display.print("Please fix range");
            display.display();
            delay(1500);
            draw();
          }
          else
          {
            jamHandler(misc_jam, "Misc Sweep Jam", nullptr, false, false, true, false, channel1, channel2);
            draw();
          }
        }
      }
    }
    delay(1);
  }
}

void misc()
{
  static const char *labels[] = {
      "Custom Sweep Range",
      "Full Band (Ch 0-125)",
      "ISM Band (Ch 1-84)",
      "WiFi 2.4G (Ch 1-14)",
      "Mode: CW Carrier"
  };
  static const int icons[] = {
      ICON_SETTINGS, ICON_RF, ICON_RF, ICON_WIFI, ICON_MISC
  };
  const int itemCount = 5;
  int       current = 0;
  uint32_t  lastScrollTime = 0;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("Misc Jammer");
    labels[4] = (misc_jam_method == 0) ? "Mode: CW Carrier" : "Mode: Packet Noise";
    drawMenuList(labels, icons, itemCount, current);
    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();

    if ( btnNext.isSingle() )
    {
      current = (current + 1) % itemCount;
      draw();
    }
    if ( btnPrevious.isSingle() )
    {
      current = (current - 1 + itemCount) % itemCount;
      draw();
    }
    if ( btnOK.isHolded() )
    {
      updateDisplay(menu_number);
      return;
    }

    uint32_t now = millis();
    if ( now - lastScrollTime >= 250 )
    {
      lastScrollTime = now;
      draw();
    }

    if ( btnOK.isSingle() )
    {
      switch ( current )
      {
      case 0:
        misc_custom_sweep();
        break;
      case 1:
        jamHandler(misc_jam, "Full 2.4G Sweep", nullptr, false, false, true, false, 0, 125);
        break;
      case 2:
        jamHandler(misc_jam, "ISM Band Sweep", nullptr, false, false, true, false, 1, 84);
        break;
      case 3:
        jamHandler(misc_jam, "WiFi Band Sweep", nullptr, false, false, true, false, 1, 14);
        break;
      case 4:
        misc_jam_method = (misc_jam_method == 0) ? 1 : 0;
        prefs.putInt("misc_configs", misc_jam_method);
        draw();
        break;
      }
      draw();
    }
    delay(1);
  }
}

// ---------------------------------------------------------------------------
// Peripheral jammer sub-menu (HID / RC)
// ---------------------------------------------------------------------------
// Same joystick gestures as the rest of the UI:
//   Next / Previous  -> move the highlight
//   OK single click  -> start the highlighted jam (OK again stops it)
//   OK long press    -> back to the main menu
// ---------------------------------------------------------------------------
static void runJamMenu(const char *title, const char *const *labels, const int *icons, void (*const *actions)(), int itemCount)
{
  int current = 0;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame(title);
    drawMenuList(labels, icons, itemCount, current);
    drawMenuHint();
    display.display();
  };

  draw();

  uint32_t lastScrollTime = 0;

  while ( true )
  {
    updateJoystickButtons();

    if ( btnNext.isSingle() )
    {
      current = (current + 1) % itemCount;
      draw();
    }
    if ( btnPrevious.isSingle() )
    {
      current = (current - 1 + itemCount) % itemCount;
      draw();
    }
    if ( btnOK.isHolded() )
    {
      updateDisplay(menu_number);
      return;
    }

    uint32_t now = millis();
    if ( now - lastScrollTime >= 250 )
    {
      lastScrollTime = now;
      draw();
    }

    if ( btnOK.isSingle() )
    {
      // CW = true is essential: without a constant carrier these jammers only
      // fire isolated packets and produce no usable interference.
      jamHandler(actions[current], String(title) + " - " + String(labels[current]), nullptr, true, false);
      draw();
    }
    delay(1);
  }
}

void hid_select()
{
  static const char *const labels[]  = {"All (ch 2-72)", "Logitech Unify", "Microsoft HID"};
  static const int         icons[]   = {ICON_HID, ICON_MISC, ICON_USB};
  static void (*const actions[])()   = {hid_wireless_jam, logitech_unifying_jam, ms_wireless_jam};
  runJamMenu("HID Jammer", labels, icons, actions, 3);
}

void rc_select()
{
  static const char *const labels[]  = {"All (ch 0-125)", "FlySky AFHDS", "Spektrum DSM", "FrSky D8/D16"};
  static const int         icons[]   = {ICON_RC, ICON_DRONE, ICON_RC, ICON_RC};
  static void (*const actions[])()   = {rc_controller_jam, flysky_jam, spektrum_jam, frsky_jam};
  runJamMenu("RC Jammer", labels, icons, actions, 4);
}

// ---------------------------------------------------------------------------
// Tools sub-menu — diagnostics reachable from the joystick so the OLED covers
// the same functions the serial/web UI have (self-test, module scan/save, logs).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Tools sub-menu & styled diagnostic sub-screens
// ---------------------------------------------------------------------------
static void toolsRfTestScreen()
{
  display.clearDisplay();
  drawMenuFrame("RF Self-Test");
  display.setTextColor(WHITE, BLACK);
  display.setCursor(8, 28);
  display.print("Checking Radios...");
  display.display();

  int det = 0, tot = 0;
  bool ok = rfTest(&det, &tot);

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("RF Self-Test");

    display.fillRect(2, 13, SCREEN_WIDTH - 4, 9, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(6, 14);
    display.print("Status: ");
    display.print(det);
    display.print("/");
    display.print(tot);
    display.print(" ");
    display.print(ok ? "[PASS]" : "[FAIL]");

    display.drawRoundRect(2, 24, SCREEN_WIDTH - 4, 29, 2, WHITE);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(6, 27);
    display.print("Signal : 2435 MHz CW");
    display.setCursor(6, 35);
    display.print("Radios : ");
    display.print(det);
    display.print(" Active");
    display.setCursor(6, 44);
    display.print(ok ? "Health : Hardware OK" : "Health : Check Module");

    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();
    if ( btnOK.isSingle() || btnOK.isHolded() )
      break;
    delay(1);
  }
}

static void toolsScanSaveScreen()
{
  display.clearDisplay();
  drawMenuFrame("nRF24 Scan/Save");
  display.setTextColor(WHITE, BLACK);
  display.setCursor(8, 28);
  display.print("Scanning Bus Pins...");
  display.display();

  handlenRF24Scan();

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("nRF24 Scan/Save");

    display.fillRect(2, 13, SCREEN_WIDTH - 4, 9, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(6, 14);
    display.print("Saved: ");
    display.print(nrf24_count);
    display.print(" Radios [OK]");

    display.drawRoundRect(2, 24, SCREEN_WIDTH - 4, 29, 2, WHITE);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(6, 27);
    display.print("Bus     : HSPI 10MHz");
    display.setCursor(6, 35);
    display.print("CSN Pins: ");
    for ( int i = 0; i < nrf24_count && i < 4; i++ )
    {
      display.print(csn_pins[i]);
      if ( i < nrf24_count - 1 && i < 3 ) display.print(",");
    }
    display.setCursor(6, 44);
    display.print("Storage : Flash NVS");

    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();
    if ( btnOK.isSingle() || btnOK.isHolded() )
      break;
    delay(1);
  }
}

static void toolsWifiScanScreen()
{
  display.clearDisplay();
  drawMenuFrame("WiFi AP Scanner");
  display.setTextColor(WHITE, BLACK);
  display.setCursor(8, 28);
  display.print("Scanning 2.4G APs...");
  display.display();

  if ( access_point == 0 )
    WiFi.softAPdisconnect();
  WiFi.mode(WIFI_STA);
  delay(100);
  int nets = scan_wifi_APs(WiFiScanChannels, false);
  if ( access_point == 0 )
    WiFi.softAP(ssid.c_str(), password.c_str());

  int scrollOffset = 0;
  const int maxVisible = 4;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("WiFi AP Scanner");

    display.fillRect(2, 13, SCREEN_WIDTH - 4, 9, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(6, 14);
    display.print("Found: ");
    display.print(nets);
    display.print(" APs on 2.4G");

    if ( nets == 0 )
    {
      display.setTextColor(WHITE, BLACK);
      display.setCursor(8, 32);
      display.print("No WiFi APs found");
    }
    else
    {
      display.setTextSize(1);
      display.setTextColor(WHITE, BLACK);
      for ( int i = 0; i < maxVisible; i++ )
      {
        int ch = scrollOffset + i + 1;
        if ( ch > 14 ) break;

        int y = 24 + i * 8;
        display.setCursor(4, y);
        display.print("Ch");
        if ( ch < 10 ) display.print("0");
        display.print(ch);
        display.print(": ");
        display.print(WiFiScanChannels[ch - 1]);
        display.print(" [");
        String ap = APs_array[ch - 1];
        int nl = ap.indexOf('\n');
        if ( nl > 0 ) ap = ap.substring(0, nl);
        if ( ap.length() > 8 ) ap = ap.substring(0, 8);
        if ( ap.length() == 0 ) ap = "-";
        display.print(ap);
        display.print("]");
      }
    }

    if ( scrollOffset > 0 )
    {
      display.setCursor(120, 24); display.print("^");
    }
    if ( scrollOffset + maxVisible < 14 )
    {
      display.setCursor(120, 48); display.print("v");
    }

    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(4, 55);
    display.print("< > Scroll | OK: Exit");

    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();
    if ( btnNext.isSingle() && scrollOffset + maxVisible < 14 )
    {
      scrollOffset++;
      draw();
    }
    if ( btnPrevious.isSingle() && scrollOffset > 0 )
    {
      scrollOffset--;
      draw();
    }
    if ( btnOK.isSingle() || btnOK.isHolded() )
      break;
    delay(1);
  }
}

static void toolsBleScanScreen()
{
  display.clearDisplay();
  drawMenuFrame("BLE Scanner");
  display.setTextColor(WHITE, BLACK);
  display.setCursor(8, 28);
  display.print("Scanning BLE...");
  display.display();

  int n = scan_BLE();

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("BLE Scanner");

    display.fillRect(2, 13, SCREEN_WIDTH - 4, 9, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(6, 14);
    display.print("Found: ");
    display.print(n);
    display.print(" BLE Devices");

    display.drawRoundRect(2, 24, SCREEN_WIDTH - 4, 29, 2, WHITE);
    display.setTextColor(WHITE, BLACK);
    display.setCursor(6, 27);
    display.print("Radio   : ESP32 BLE 4.2");
    display.setCursor(6, 35);
    display.print("Packets : Adv Beacons");
    display.setCursor(6, 44);
    display.print("Output  : Serial Dump");

    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();
    if ( btnOK.isSingle() || btnOK.isHolded() )
      break;
    delay(1);
  }
}

void tools_select()
{
  static const char *const labels[] = {
      "RF self-test",       "nRF24 scan/status",   "nRF24 scan + save",
      "WiFi scan",          "BLE scan",            "Reboot"
  };
  static const int icons[] = {
      ICON_RF, ICON_SCAN, ICON_SETTINGS, ICON_WIFI, ICON_BLE, ICON_REBOOT
  };
  const int N = sizeof(labels) / sizeof(labels[0]);
  int       current = 0;
  uint32_t  lastScrollTime = 0;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("Tools");
    drawMenuList(labels, icons, N, current);
    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();

    if ( btnNext.isSingle() )      { current = (current + 1) % N; draw(); }
    if ( btnPrevious.isSingle() )  { current = (current - 1 + N) % N; draw(); }
    if ( btnOK.isHolded() )        { updateDisplay(menu_number); return; }

    uint32_t now = millis();
    if ( now - lastScrollTime >= 250 )
    {
      lastScrollTime = now;
      draw();
    }

    if ( btnOK.isSingle() )
    {
      switch ( current )
      {
      case 0:
        toolsRfTestScreen();
        break;
      case 1:
        nrfStatusScreen();
        break;
      case 2:
        toolsScanSaveScreen();
        break;
      case 3:
        toolsWifiScanScreen();
        break;
      case 4:
        toolsBleScanScreen();
        break;
      case 5:
        display.clearDisplay();
        drawMenuFrame("System");
        display.setTextColor(WHITE, BLACK);
        display.setCursor(20, 26);
        display.print("Rebooting ESP32...");
        display.display();
        delay(500);
        ESP.restart();
        break;
      }
      draw();
    }
    delay(1);
  }
}

void ble_select()
{
  static const char *const labels[] = {"BLE Advert Jam", "BLE Data Jam"};
  static const int         icons[]  = {ICON_BLE, ICON_RF};
  int                      current  = 0;
  uint32_t                 lastScrollTime = 0;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("BLE Jammer");
    drawMenuList(labels, icons, 2, current);
    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();

    if ( btnNext.isSingle() )
    {
      current = (current + 1) % 2;
      draw();
    }
    if ( btnPrevious.isSingle() )
    {
      current = (current - 1 + 2) % 2;
      draw();
    }
    if ( btnOK.isHolded() )
    {
      updateDisplay(menu_number);
      return;
    }

    uint32_t now = millis();
    if ( now - lastScrollTime >= 250 )
    {
      lastScrollTime = now;
      draw();
    }

    if ( btnOK.isSingle() )
    {
      if ( current == 0 )
        jamHandler(ble_advertising_jam, String("BLE AD Jam"), nullptr, true, true);
      else
        jamHandler(ble_data_jam, String("BLE Data Jam"), nullptr, true, false);
      draw();
    }
    delay(1);
  }
}

void wifi_select()
{
  static const char *const labels[] = {
      "All Ch (Jam+Deauth)",
      "Target Ch (Scan&Pick)",
      "Active Ch (Smart Jam)",
      "Beacon Spam (List)",
      "Beacon Spam (Random)"
  };
  static const int icons[] = {
      ICON_WIFI, ICON_SCAN, ICON_RF, ICON_MISC, ICON_MISC
  };
  const int itemCount = 5;
  int current = 0;
  uint32_t lastScrollTime = 0;

  auto draw = [&]() {
    display.clearDisplay();
    drawMenuFrame("WiFi Attacks");
    drawMenuList(labels, icons, itemCount, current);
    drawMenuHint();
    display.display();
  };

  draw();

  while ( true )
  {
    updateJoystickButtons();

    if ( btnNext.isSingle() )
    {
      current = (current + 1) % itemCount;
      draw();
    }
    if ( btnPrevious.isSingle() )
    {
      current = (current - 1 + itemCount) % itemCount;
      draw();
    }
    if ( btnOK.isHolded() )
    {
      updateDisplay(menu_number);
      return;
    }

    uint32_t now = millis();
    if ( now - lastScrollTime >= 250 )
    {
      lastScrollTime = now;
      draw();
    }

    if ( btnOK.isSingle() )
    {
      switch ( current )
      {
      case 0: // All Channels
      {
        jamHandler(wifi_jam, String("WiFi Jam All"), nullptr, true, true);
        attackHandler(String("WiFi Deauth All"), wifi_deauth_all, nullptr);
        break;
      }
      case 1: // Target Channel Scan & Pick
      {
        display.clearDisplay();
        drawMenuFrame("WiFi Scanner");
        display.setTextColor(WHITE, BLACK);
        display.setCursor(10, 26);
        display.print("Scanning APs...");
        display.display();

        if ( access_point == 0 )
          WiFi.softAPdisconnect();
        WiFi.mode(WIFI_STA);
        delay(100);
        scan_wifi_APs(WiFiScanChannels, false);
        if ( access_point == 0 )
          WiFi.softAP(ssid.c_str(), password.c_str());

        int targetCh = 1;
        bool inChannelPicker = true;
        auto drawPicker = [&]() {
          display.clearDisplay();
          drawMenuFrame("Target WiFi Ch");

          display.drawRoundRect(2, 14, SCREEN_WIDTH - 4, 38, 3, WHITE);
          display.setTextColor(WHITE, BLACK);
          display.setCursor(8, 18);
          display.print("< Channel: ");
          if ( targetCh < 10 ) display.print(" ");
          display.print(targetCh);
          display.print(" >");

          display.setCursor(8, 28);
          display.print("APs Detected: ");
          display.print(WiFiScanChannels[targetCh - 1]);

          display.setCursor(8, 38);
          String apSample = APs_array[targetCh - 1];
          int nl = apSample.indexOf('\n');
          if ( nl > 0 ) apSample = apSample.substring(0, nl);
          if ( apSample.length() > 17 ) apSample = apSample.substring(0, 17);
          if ( apSample.length() == 0 ) apSample = "(No APs detected)";
          display.print(apSample);

          drawMenuHint();
          display.display();
        };

        drawPicker();

        while ( inChannelPicker )
        {
          updateJoystickButtons();
          if ( btnNext.isSingle() )
          {
            targetCh++;
            if ( targetCh > 14 ) targetCh = 1;
            drawPicker();
          }
          if ( btnPrevious.isSingle() )
          {
            targetCh--;
            if ( targetCh < 1 ) targetCh = 14;
            drawPicker();
          }
          if ( btnOK.isHolded() )
          {
            inChannelPicker = false;
            break;
          }
          if ( btnOK.isSingle() )
          {
            jamHandler(wifi_channel, "Jamming Ch " + String(targetCh), nullptr, true, true, false, false, targetCh - 1, 0);
            wifi_deauth_channel(targetCh);
            break;
          }
          delay(1);
        }
        break;
      }
      case 2: // Active Channels Only (Smart Attack)
      {
        display.clearDisplay();
        drawMenuFrame("Active Ch Scan");
        display.setTextColor(WHITE, BLACK);
        display.setCursor(10, 26);
        display.print("Scanning Channels...");
        display.display();

        if ( access_point == 0 )
          WiFi.softAPdisconnect();
        WiFi.mode(WIFI_STA);
        delay(100);
        NumberChannels = scan_wifi_APs(WiFiScanChannels, true);
        if ( access_point == 0 )
          WiFi.softAP(ssid.c_str(), password.c_str());

        jamHandler(wifi_scan_jam, String("WiFi Smart Jam"), nullptr, true, true);
        attackHandler(String("WiFi Deauth Scan"), wifi_deauth_scan, nullptr);
        break;
      }
      case 3: // Beacon Spam (List)
      {
        attackHandler(String("WiFi Beacon Spam"), wifi_beacon_spam_array, nullptr);
        break;
      }
      case 4: // Beacon Spam (Random)
      {
        attackHandler(String("WiFi Beacon Spam"), wifi_beacon_spam_random, nullptr);
        break;
      }
      }
      draw();
    }
    delay(1);
  }
}

static void showBiosBootScreen()
{
  static const char *const biosLines[] = {
      "CPU: ESP32 @ 240MHz",
      "RAM: 320KB | ROM: 4MB",
      "HSPI: Bus 10MHz  [OK]",
      "nRF24: Active    [OK]",
      "WiFi : 802.11bgn [OK]",
      "BLE  : 4.2 Stack [OK]",
      "SYS  : Guard ON  [OK]",
      "BOOT : Starting OS..."
  };
  const int totalLines = sizeof(biosLines) / sizeof(biosLines[0]);
  const int maxVisible = (SCREEN_HEIGHT == 32) ? 2 : 4;

  for ( int step = 1; step <= totalLines; step++ )
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);

    if ( SCREEN_HEIGHT == 32 )
    {
      int startIdx = (step > maxVisible) ? (step - maxVisible) : 0;
      int count = (step > maxVisible) ? maxVisible : step;
      for ( int i = 0; i < count; i++ )
      {
        int lineIdx = startIdx + i;
        display.setCursor(4, 2 + i * 10);
        display.print("> ");
        display.print(biosLines[lineIdx]);
      }
      display.setCursor(4, 24);
      display.print("___.: By Lucx :.____");
    }
    else
    {
      display.setCursor(4, 2);
      display.print("* Mee_BIOS v2.4 *");
      display.drawFastHLine(0, 11, SCREEN_WIDTH, WHITE);

      int startIdx = (step > maxVisible) ? (step - maxVisible) : 0;
      int count = (step > maxVisible) ? maxVisible : step;

      for ( int i = 0; i < count; i++ )
      {
        int lineIdx = startIdx + i;
        int y = 14 + i * 9;
        display.setCursor(4, y);
        display.print("> ");
        display.print(biosLines[lineIdx]);
      }

      display.drawFastHLine(0, 52, SCREEN_WIDTH, WHITE);
      display.setCursor(4, 55);
      display.print("___.: By Lucx :.____");
    }

    display.display();
    delay(200);
  }
  delay(400);
}

void setup()
{
  Serial.begin(115200);

  prefs.begin("configs", false);

  disableCore0WDT();

  // Protect GPIO 12 (MTDI strap) from being read HIGH at reboot
  pinMode(12, INPUT_PULLDOWN);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  handlenRF24Init();
  if ( nrf24_count == 0 )
    handlenRF24Scan();

  // Restore every persisted setting (PA level, dwell, screen height, sweep
  // mode, AP mode) BEFORE anything uses them.
  loadSettings();

  if ( !prefs.isKey("SSIDs_Array") )
    {
      memset(SSIDs_Array, 0, sizeof(SSIDs_Array));
      for ( int i = 0; i < sizeof(default_SSIDs_Array) / sizeof(default_SSIDs_Array[0]) && i < 100; i++ )
      {
        if ( default_SSIDs_Array[i] == nullptr )
          break;
        strlcpy(SSIDs_Array[i], default_SSIDs_Array[i], 33);
      }
      prefs.putBytes("SSIDs_Array", SSIDs_Array, sizeof(SSIDs_Array));
    }


  if ( access_point == 0 )
  {
    ssid     = prefs.getString("ssid", default_ssid);
    password = prefs.getString("password", default_password);

    WiFi.mode(WIFI_AP);
    delay(100);

    if ( WiFi.softAP(ssid.c_str(), password.c_str()) )
    {
      IPAddress apIp = WiFi.softAPIP();
      if ( apIp != IPAddress((uint32_t)0) )
      {
        dnsServerStarted = dnsServer.start(53, "*", apIp);
      }
    }

    server.on("/", handleRoot);
    server.on("/bluetooth_jam", []() { jamHandler(bluetooth_jam, String("Bluetooth Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_bluetooth_jam_64 : bitmap_bluetooth_jam_32), true, false); });
    server.on("/drone_jam", []() { jamHandler(drone_jam, String("Drone Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_drone_jam_64 : bitmap_drone_jam_32), true, false); });
    server.on("/wifi_jam", []() { jamHandler(wifi_jam, String("WiFi Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_wifi_jam_64 : bitmap_wifi_jam_32), false, false); });
    server.on("/wifi_deauth_all", []() { attackHandler(String("WiFi Deauthing"), wifi_deauth_all, decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_wifi_deauth_64 : bitmap_wifi_deauth_32)); });
    server.on("/wifi_scan_jam", []() { jamHandler(wifi_scan_jam, String("WiFi Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_wifi_jam_64 : bitmap_wifi_jam_32), false, false, false, true); });
    server.on("/wifi_deauth_scan", []() { attackScanHandler(String("WiFi Deauthing"), wifi_deauth_scan, decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_wifi_deauth_64 : bitmap_wifi_deauth_32)); });
    server.on("/wifi_random_spam", []() { attackScanHandler(String("WiFi Beacon Spam"), wifi_beacon_spam_random, decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_beacon_spaming_64 : bitmap_beacon_spaming_32)); });
    server.on("/wifi_array_spam", []() { attackScanHandler(String("WiFi Beacon Spam"), wifi_beacon_spam_array, decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_beacon_spaming_64 : bitmap_beacon_spaming_32)); });
    server.on("/ble_advertising_jam", []() { jamHandler(ble_advertising_jam, String("BLE Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_ble_jam_64 : bitmap_ble_jam_32), true, false); });
    server.on("/ble_data_jam", []() { jamHandler(ble_data_jam, String("BLE Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_ble_jam_64 : bitmap_ble_jam_32), true, false); });
    server.on("/zigbee_jam", []() { jamHandler(zigbee_jam, String("Zigbee Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_zigbee_jam_64 : bitmap_zigbee_jam_32), false, false); });

    // HID / RC / Apple were previously only reachable from the WebSerial
    // console.  These routes give the browser (and curl / a script) the same
    // entry points as the terminal and the joystick.
    server.on("/hid_jam", []() { jamHandler(hid_wireless_jam, String("HID Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_misc_jammer_64 : bitmap_misc_jammer_32), true, false); });
    server.on("/rc_jam", []() { jamHandler(rc_controller_jam, String("RC Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_drone_jammer_64 : bitmap_drone_jammer_32), true, false); });
    server.on("/ble_apple", []() { jamHandler(ble_apple_flood_task, String("Apple BLE Flood"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_ble_jammer_64 : bitmap_ble_jammer_32), false, false); });
    server.on("/web_serial", []() { sendHtmlAndExecute(html_webserial, html_webserial_len); });
    server.on("/misc_jam", miscChannelsHandler);
    server.on("/wifi_selected_jam", wifiChannelsHandler);
    server.on("/wifi_selected_deauth", wifiDeauthChannelsHandler);
    server.on("/rescan", RescanHandler);

    server.on("/api/status", handleApiStatus);
    server.on("/api/settings", handleApiSettings);
    server.on("/api/version", handleApiVersion);

    server.on("/settings", []() {
      server.sendHeader("Content-Encoding", "gzip");
      server.send_P(200, "text/html", (const char *)html_settings, html_settings_len);
    });
    server.on("/OTA", []() {
      server.sendHeader("Content-Encoding", "gzip");
      server.send_P(200, "text/html", (const char *)html_ota, html_ota_len);
    });

    server.on("/WebCommand", HandleWebCommand);

    server.on("/generate_204", handleRoot);
    server.on("/redirect", handleRoot);
    server.on("/hotspot-detect.html", handleRoot);
    server.on("/canonical.html", handleRoot);
    server.on("/ncsi.txt", handleRoot);

    server.on(
        "/update", HTTP_POST, []() {
          if ( Update.end(true) )
          {
            Serial.printf("Update Success: %u bytes\n", Update.size());
            server.send(200, "text/plain", "Update Success");
            delay(100);
            display.clearDisplay();
            display.drawBitmap(0, 0, decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_pls_reboot_64 : bitmap_pls_reboot_32), SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
            display.display();
            delay(2000);
            ESP.restart();
          }
          else
          {
            Update.printError(Serial);
            server.send(500, "text/plain", "Update Failed");
          }
        },
        handleFileUpload
    );

    server.on("/set_settings", []() { handlerSetSettings(); });

    server.begin();
    webServerStarted = true;
  }
  Serial.println(logotype + "\n\n");

  initJoystick();
  buttons = 2; // Joystick uses 3-way navigation (OK = SW, Next, Previous)

  btnOK.setTickMode(false);
  btnNext.setTickMode(false);
  btnPrevious.setTickMode(false);
  btnOK.setClickTimeout(200);
  btnNext.setClickTimeout(200);
  btnPrevious.setClickTimeout(200);
  btnOK.setTimeout(600);
  btnNext.setTimeout(600);
  btnPrevious.setTimeout(600);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  if ( SCREEN_HEIGHT == 32 )
  {
    display.ssd1306_command(SSD1306_SETCOMPINS);
    display.ssd1306_command(0x02);
    display.ssd1306_command(SSD1306_SETMULTIPLEX);
    display.ssd1306_command(0x1F);
  }
  else
  {
    display.ssd1306_command(SSD1306_SETCOMPINS);
    display.ssd1306_command(0x12);
    display.ssd1306_command(SSD1306_SETMULTIPLEX);
    display.ssd1306_command(0x3F);
  }

  display.setTextColor(WHITE);
  display.setTextSize(1);
  if ( display_setting )
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
  showBiosBootScreen();

  Serial.println("  Scanning Wi-Fi APs");
  int networks = scan_wifi_APs(WiFiScanChannels, false);
  Serial.println("  Finded " + String(networks) + " APs");

  Serial.println("  help ==> Displays a list of available commands\n");

  updateDisplay(0);
}

void executeAction(int menuNum)
{
  if ( menuNum == 6 )
  {
    settings();
    updateDisplay(menu_number);
    return;
  }
  if ( menuNum == 2 )
  {
    wifi_select();
    updateDisplay(menu_number);
    return;
  }
  if ( menuNum == 12 )
  {
    // Apple Continuity flood runs on the ESP32's own BLE radio, so no nRF24 is required.
    jamHandler(ble_apple_flood_task, String("Apple BLE Flood"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_ble_jammer_64 : bitmap_ble_jammer_32), false, false);
    updateDisplay(menu_number);
    return;
  }
  if ( menuNum == 5 )
  {
    misc();
    updateDisplay(menu_number);
    return;
  }
  if ( menuNum == 3 )
  {
    ble_select();
    updateDisplay(menu_number);
    return;
  }
  if ( menuNum == 10 )
  {
    hid_select();
    updateDisplay(menu_number);
    return;
  }
  if ( menuNum == 11 )
  {
    rc_select();
    updateDisplay(menu_number);
    return;
  }
  if ( menuNum == 13 )
  {
    tools_select();
    updateDisplay(menu_number);
    return;
  }

  if ( nrf24_count <= 0 )
  {
    display.clearDisplay();
    display.setCursor(40, 0);
    display.println("WARNING");

    display.setCursor(5, 10);
    display.println("nRF24 not configured");

    display.setCursor(7, 20);
    display.println("Configure in web UI");

    display.display();
    delay(1500);
    updateDisplay(menu_number);
    return;
  }

  switch ( menu_number )
  {
  case 0:
    jamHandler(bluetooth_jam, String("Bluetooth Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_bluetooth_jam_64 : bitmap_bluetooth_jam_32), true, false);
    break;
  case 1:
    jamHandler(drone_jam, String("Drone Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_drone_jam_64 : bitmap_drone_jam_32), true, false);
    break;
  case 4:
    jamHandler(zigbee_jam, String("Zigbee Jamming"), decodeLZSS((SCREEN_HEIGHT == 64) ? bitmap_zigbee_jam_64 : bitmap_zigbee_jam_32), false, false);
    break;
  case 7:
    jamHandler(full_24g_jam, String("Full 2.4 GHz"), nullptr, false, false);
    break;
  case 8:
    jamHandler(usb_24g_jam, String("USB 2.4 GHz"), nullptr, false, false);
    break;
  case 9:
    jamHandler(fpv_video_jam, String("FPV / Video"), nullptr, false, false);
    break;
  default:
    break;
  }
  updateDisplay(menu_number);
}

void loop()
{
updateJoystickButtons();

  SerialCommands();

  if ( access_point == 0 )
  {
    if ( webServerStarted )
    {
      server.handleClient();
    }
    if ( dnsServerStarted )
    {
      dnsServer.processNextRequest();
    }
  }
  if ( buttons == 0 )
  {
    if ( btnOK.isSingle() )
    {
      menu_number = (menu_number + 1) % 14;
      updateDisplay(menu_number);
    }
    if ( btnOK.isHolded() )
    {
      executeAction(menu_number);
      btnOK.resetStates();
    }
  }
  else if ( buttons == 1 )
  {
    if ( btnOK.isSingle() )
    {
      executeAction(menu_number);
    }
    if ( btnNext.isSingle() )
    {
      menu_number = (menu_number + 1) % 14;
      updateDisplay(menu_number);
    }
  }
  else if ( buttons == 2 )
  {
    if ( btnOK.isSingle() )
    {
      executeAction(menu_number);
    }

    if ( btnNext.isSingle() )
    {
      menu_number = (menu_number + 1) % 14;
      updateDisplay(menu_number);
    }

    if ( btnPrevious.isSingle() )
    {
      menu_number = (menu_number - 1 + 14) % 14;
      updateDisplay(menu_number);
    }
  }
}
