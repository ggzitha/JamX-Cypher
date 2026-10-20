# Wiring — nRF24 Jammer (esp32-wroom32U / "Flexible" firmware)

This matches the current firmware in `esp32-wroom32U/src/Flexible/`.
Three nRF24 modules share **one** SPI bus (HSPI). Each module gets its own CE and CSN.

> ⚠️ **Three modules must be configured with:**
> ```
> set nrf24 16,15 18,17 23,19
> ```
> (web UI → Settings → nRF24 Modules Setup, or the serial command above)

---

## Pin map

### Shared SPI bus (all three modules)

| Signal | ESP32 GPIO |
|:---:|:---:|
| SCK  | **14** |
| MOSI | **13** |
| MISO | **12** |

All three modules' `SCK → 14`, `MOSI → 13`, `MISO → 12`. They are not separate buses.

### Per-module control pins

| Module | CE | CSN |
|:---:|:---:|:---:|
| Module 0 | **16** | **15** |
| Module 1 | **18** | **17** |
| Module 2 | **23** | **19** |

> Note: 18 / 19 / 23 are used here as **plain GPIO**, not as VSPI.
> Do **not** wire MISO/MOSI/SCK to those pins — the bus is HSPI (14/13/12).

### nRF24 module power

| nRF24 pin | Connect |
|:---:|:---|
| VCC | **3.3 V** (external ≥ 3 A rail) |
| GND | **GND** |
| IRQ | leave unconnected |

* One **100 µF electrolytic + 100 nF ceramic** capacitor across VCC—GND at every module.
* Do **not** feed the three modules from the ESP32 board's own 3.3 V regulator.

### ⚠️ Required: 10 kΩ pull-down on each CSN

This is the fix that made the build actually work.
Each **CSN** (15, 17, 19) gets a **10 kΩ resistor to GND**:

```
CSN ── 10kΩ ── GND
```

Without these, multiple modules on a shared SPI bus can enter a bad state
because the CSN lines float while other modules are being addressed.

Recommended: also keep a **10 kΩ pulldown on GPIO 12 (MISO)** — it is the MTDI
strapping pin and must read LOW at boot, or the ESP32 can fail with
`1.8V flash read err 1000`.

### OLED (SSD1306, I2C)

| OLED pin | ESP32 GPIO |
|:---:|:---:|
| SDA | **21** |
| SCL | **22** |
| VCC | 3.3 V |
| GND | GND |

### Joystick (3-button equivalent)

| Joystick pin | ESP32 GPIO | Function |
|:---:|:---:|:---|
| SW (button) | **25** | OK / select / stop jam |
| VRy (analog) | **35** | pushed down → Next menu |
| VRx (analog) | **34** | pushed up → Previous menu |
| +5V / VCC | 3.3 V (see your module) | |
| GND | GND | |

`VRx`/`VRy` are on ADC1 (34/35) so they still read while Wi-Fi is active.
`SW` uses the internal pull-up.

### Activity LED

| LED | ESP32 GPIO |
|:---:|:---:|
| Anode (via 220–470 Ω) | **2** |
| Cathode | GND |

---

## ASCII schematic

```
                            ESP32-WROOM-32 (dev board)
                          ┌────────────────────────────────────┐
                          │                                    │
   3.3 V rail (5 A) ──────┤ 3V3                   GND ──────────┼─ GND
                          │                                    │
              SCK ────────┤ GPIO 14                             │
             MOSI ────────┤ GPIO 13                             │
             MISO ────────┤ GPIO 12   ←── 10kΩ ── GND (strap)   │
                          │                                    │
      OLED SDA ───────────┤ GPIO 21                             │
      OLED SCL ───────────┤ GPIO 22                             │
                          │                                    │
      JOY SW   ───────────┤ GPIO 25   (internal pull-up)        │
      JOY VRy  ───────────┤ GPIO 35   ADC1                      │
      JOY VRx  ───────────┤ GPIO 34   ADC1                      │
      LED      ───────────┤ GPIO 2                              │
                          │                                    │
      Module0 CE ─────────┤ GPIO 16                             │
      Module0 CSN ────────┤ GPIO 15   ←── 10kΩ ── GND           │
      Module1 CE ─────────┤ GPIO 18                             │
      Module1 CSN ────────┤ GPIO 17   ←── 10kΩ ── GND           │
      Module2 CE ─────────┤ GPIO 23                             │
      Module2 CSN ────────┤ GPIO 19   ←── 10kΩ ── GND           │
                          └────────────────────────────────────┘
```

### Each nRF24 module (E01-2G4M27D or nRF24L01+PA+LNA)

```
        ┌──────────────────────────────┐
        │          nRF24 #N            │
        │                              │
 VCC ───│ 1  VCC  ── 100µF ── GND      │
 GND ───│ 2  GND                       │
        │ 3  CE   ←── module CE pin    │            (N=0: 16, N=1: 18, N=2: 23)
        │ 4  CSN  ←── module CSN pin   │ 10kΩ→GND  (N=0: 15, N=1: 17, N=2: 19)
 SCK ───│ 5  SCK  ←── GPIO 14          │
 MOSI ──│ 6  MOSI ←── GPIO 13          │
 MISO ──│ 7  MISO ──→ GPIO 12          │
        │ 8  IRQ  (unconnected)        │
        │      [antenna / IPEX]        │
        └──────────────────────────────┘
```

### Power budget

Three 27 dBm PA modules peak near **1.5 A together at 3.3 V**.
Use a dedicated **5 A / 3.3 V** supply (e.g. adjustable buck to 3.3 V),
thick short power wires, and bulk capacitance per module.

---

## Control reference

| Action | Command / Joystick |
|:---|:---|
| Verify radios emit | `rftest` |
| Check configured pins | `nrf24 status` |
| Set the 3 modules | `set nrf24 16,15 18,17 23,19` |
| Max PA | `set pa 3` |
| Bluetooth jam | `jam bt` (then `set bt 2` for all 80 ch) |
| BLE advertising jam (3 radios → 3 BLE channels) | `jam ble ad` |
| Single-frequency demo (Wi-Fi ch 6 = 2437 MHz) | `set sweep 1` then `jam misc 37 37` |
| Stop | `stop` (or press joystick OK) |