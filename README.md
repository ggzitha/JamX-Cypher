# 🔥 CYPHER BOX — 2.4GHz Multi-Protocol Jammer

![Platform](https://img.shields.io/badge/platform-ESP32--C3-blue)
![Radio](https://img.shields.io/badge/radio-2x%20NRF24L01%2B-green)
![Display](https://img.shields.io/badge/display-SSD1306%20128x64-orange)
![License](https://img.shields.io/badge/license-MIT-lightgrey)
![Status](https://img.shields.io/badge/status-active-success)

A pocket-sized, dual-radio 2.4 GHz multi-protocol RF testing tool built on the **ESP32-C3**. It combines **two NRF24L01+ PA+LNA modules**, a **128×64 SSD1306 OLED**, and a 3-button interface to deliver Bluetooth Classic, BLE, WiFi, and Drone-band testing in one handheld device.

> ⚠️ **FOR EDUCATIONAL AND LABORATORY TESTING ONLY.** Jamming radio communications is illegal in most countries. Use only inside an RF-shielded environment or on hardware you personally own, in a jurisdiction where this is permitted. The author is not responsible for misuse.

---

## 📖 Table of Contents

- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Wiring Diagram](#-wiring-diagram)
- [Installation](#-installation)
- [First Boot](#-first-boot)
- [Main Menu](#-main-menu)
- [Settings Reference](#-settings-reference)
- [Channel Mapping](#-channel-mapping)
- [Preset Library](#-preset-library)
- [Auto Mode (RPD Scanner)](#-auto-mode-rpd-scanner)
- [New Modes Explained](#-new-modes-explained)
- [Use Cases & Recipes](#-use-cases--recipes)
- [Hardware Tips & Troubleshooting](#-hardware-tips--troubleshooting)
- [Credits](#-credits)

---

## ✨ Features

- **Dual NRF24L01+ radios** — jam two channels simultaneously
- **Multi-protocol support** — Bluetooth Classic, BLE, WiFi 2.4GHz, Drone bands
- **14 curated presets** — WiFi Priority, BLE Advertising, BT Classic, Drone FHSS, USB Dongle, FPV Video, RC Control, Zigbee, and more
- **RPD-based spectrum scanner** — detects RF energy > -64 dBm on any channel
- **Auto mode** — reacts to live RF activity, concentrates jamming on the noisiest channels
- **Reactive JamX** — Radio 1 scouts for active packets, Radio 2 strikes the hot channel
- **Hybrid JamX** — Radio 1 blind hops while Radio 2 dynamically hunts and attacks loudest channel
- **Channel Picker** — spectrum scan with manual lock-on for dual-radio focused attacks
- **Targeted device modes** — Audio (speakers), HID (mouse/keyboard), Beacon (BLE tags)
- **Persistent settings** — saved to SPIFFS flash, survive power cycles
- **I2C bus recovery** — self-heals after RF-induced display lock-ups
- **Adjustable PA + data rate** — tune for maximum disruption or stealth

---

## 🛒 Hardware Requirements

| Component | Quantity | Notes |
| :--- | :---: | :--- |
| **ESP32-C3** dev board | 1 | SuperMini variant recommended (e.g. Seeed XIAO ESP32C3, ESP32-C3-DevKitM-1) |
| **NRF24L01+ PA+LNA** modules | 2 | The ones with external antenna + amplifier |
| **SSD1306 OLED 128×64** | 1 | I2C version, address `0x3C` |
| **Push buttons** | 3 | Momentary, normally open |
| **4.7 kΩ resistors** | 2 | I2C pull-ups (SDA + SCL) — **critical** |
| **100 nF ceramic capacitors** | 3 | One per NRF VCC + one on OLED VCC |
| **47 µF electrolytic capacitors** | 2 | One per NRF VCC — **critical** |
| **AMS1117-3.3V regulator** | 1 | Dedicated 3.3 V rail for NRFs — **strongly recommended** |
| **Perfboard / PCB** | 1 | — |

> **Power Warning:** Two NRF24L01+ PA+LNA modules at max power draw **~250 mA peak each**. The ESP32-C3 onboard regulator **cannot** supply this. Use an external AMS1117 (or similar) for the NRF modules.

---

## 🔌 Wiring Diagram

### Pin Assignments

| ESP32-C3 GPIO | Connected To | Notes |
| :---: | :--- | :--- |
| **GPIO 8** | OLED **SDA** | + 4.7 kΩ pull-up to 3.3 V |
| **GPIO 9** | OLED **SCL** | + 4.7 kΩ pull-up to 3.3 V |
| **GPIO 4** | NRF SPI **SCK** | Shared bus (both NRFs) |
| **GPIO 5** | NRF SPI **MISO** | Shared bus |
| **GPIO 6** | NRF SPI **MOSI** | Shared bus |
| **GPIO 20** | NRF1 **CE** | — |
| **GPIO 21** | NRF1 **CSN** | — |
| **GPIO 7** | NRF2 **CE** | — |
| **GPIO 10** | NRF2 **CSN** | — |
| **GPIO 3** | **UP** button | Other side to GND (internal pull-up) |
| **GPIO 0** | **DOWN** button | Other side to GND |
| **GPIO 1** | **SELECT** button | Other side to GND |

### ASCII Schematic

```
                        ┌───────────────────────────┐
                        │         ESP32-C3          │
                        │                           │
                        │  GPIO 8  ─────► SDA ──────┼──► OLED SDA (+ 4.7k pull-up to 3.3V)
                        │  GPIO 9  ─────► SCL ──────┼──► OLED SCL (+ 4.7k pull-up to 3.3V)
                        │                           │
                        │  GPIO 4  ─────► SCK ──────┼──┬──► NRF1 SCK
                        │  GPIO 5  ─────► MISO ─────┼──┼──► NRF1 MISO
                        │  GPIO 6  ─────► MOSI ─────┼──┼──► NRF1 MOSI
                        │  GPIO 20 ─────► CE ───────┼──┼──► NRF1 CE
                        │  GPIO 21 ─────► CSN ──────┼──┼──► NRF1 CSN
                        │                           │  │
                        │                           │  └──► NRF2 SCK, MISO, MOSI (shared)
                        │  GPIO 7  ─────► CE ───────┼─────► NRF2 CE
                        │  GPIO 10 ─────► CSN ──────┼─────► NRF2 CSN
                        │                           │
                        │  GPIO 3  ─────► UP  ──────┼──► Button ──► GND
                        │  GPIO 0  ─────► DOWN ─────┼──► Button ──► GND
                        │  GPIO 1  ─────► SEL ──────┼──► Button ──► GND
                        │                           │
                        │  3.3V   ─────► VCC rail ──┼──► OLED VCC, NRF1 VCC, NRF2 VCC
                        │  GND    ─────► GND rail ──┼──► OLED GND, NRF1 GND, NRF2 GND
                        └───────────────────────────┘

      External 3.3V rail for NRFs (recommended):
        [5V USB] ──► [AMS1117-3.3] ──┬──► 100nF ──┬──► NRF1 VCC
                                      ├──► 47µF  ─┤
                                      ├──► 100nF ──┬──► NRF2 VCC
                                      └──► 47µF  ─┘
```

### NRF24L01+ Pinout (Module Side)

```
   ┌───────────────────┐
   │  NRF24L01+ PA+LNA │
   │                   │
   │  GND  VCC  CE  CSN│
   │   │    │    │   │ │
   │   │    │    │   │ │
   │  SCK MISO MOSI IRQ│
   └───────────────────┘
```

| NRF Pin | Connect to |
| :---: | :--- |
| GND | Common ground |
| VCC | **3.3 V only** (never 5 V) + 47 µF cap to GND |
| CE | ESP32 GPIO (per radio) |
| CSN | ESP32 GPIO (per radio) |
| SCK | ESP32 GPIO 4 (shared) |
| MISO | ESP32 GPIO 5 (shared) |
| MOSI | ESP32 GPIO 6 (shared) |
| IRQ | Not connected |

---

## ⚙️ Installation

### 1. Arduino IDE Setup

Install **Arduino IDE 2.x** and add ESP32 board support:

1. **File → Preferences → Additional Board Manager URLs**:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
2. **Tools → Board → Boards Manager** → search `esp32` → install **esp32 by Espressif Systems** (v3.0.0 or newer).
3. **Tools → Board → ESP32 Arduino → ESP32C3 Dev Module**.
4. **Tools → USB CDC On Boot** → `Enabled`.
5. **Tools → CPU Frequency** → `160 MHz`.
6. **Tools → Flash Size** → `4 MB (32 Mb)`.
7. **Tools → Partition Scheme** → `Default 4MB with spiffs`.

### 2. Install Libraries

**Tools → Manage Libraries** — install each of these:

| Library | Author | Version |
| :--- | :--- | :--- |
| **RF24** | TMRh20 | ≥ 1.5.0 |
| **Adafruit SSD1306** | Adafruit | ≥ 2.5.7 |
| **Adafruit GFX Library** | Adafruit | ≥ 1.11.5 |
| **U8g2_for_Adafruit_GFX** | olikraus | ≥ 1.8.0 |
| **SPIFFS** | *(bundled with ESP32 core)* | — |
| **Wire** | *(bundled)* | — |

### 3. Flash Firmware

1. Open `JammerC-3.ino` in Arduino IDE.
2. Select the correct **COM / ttyUSB / ttyACM** port.
3. Click **Upload**.
4. If upload fails, hold **BOOT (GPIO 9)** while tapping **RESET (EN)** to enter download mode.

---

## 🚀 First Boot

On first boot you'll see:

1. **Splash screen** — "CYPHER BOX" (1.5 s)
2. **Info screen** — welcome text (2 s)
3. **Radio init** — shows `Radio 1: OK`, `Radio 2: OK`
4. **Main menu**

On very first boot, serial output may show:
```
Invalid settings magic. Using defaults.
```
This is expected — go to **Settings → Save & Exit** once to persist the defaults.

---

## 🎮 Main Menu

Navigate with **UP / DOWN**, confirm with **SELECT**. Two items are visible at a time.

| Menu Item | Function |
| :--- | :--- |
| **BT/BLE JamX** | Jams Bluetooth Classic or BLE depending on Protocol/Target settings |
| **Drone JamX** | Jams drone control bands (0-124 NRF channels) |
| **Wifi JamX** | Jams WiFi 2.4 GHz channels |
| **MultiCH JamX** | Mix of all protocols (random hop) |
| **Reactive JamX** | Scout (R1 RX) detects active channel → Striker (R2 TX) attacks it |
| **Hybrid JamX** | Radio 1 blind hops while Radio 2 dynamically hunts and attacks loudest channel |
| **Channel Picker** | Scans spectrum, lets you choose top channel, both radios lock & jam |
| **Spectrum Scan** | Scans 0-125 and draws a bar graph |
| **Test Radios** | Prints `printPrettyDetails()` for each NRF |
| **Settings** | All configuration options |
| **Help** | On-device 8-page guide |

To **stop any jam mode**, press **SELECT**.

---

## 🔧 Settings Reference

Enter **Settings** from the main menu. Use **UP / DOWN** to scroll, **SELECT** to change a value or enter an action.

### 1. `PA:` — Power Amplifier Level

Controls NRF24 output power.

| Value | Power | Range | When to use |
| :---: | :---: | :---: | :--- |
| MIN (-18 dBm) | 0.016 mW | ~5 cm | Testing wiring only |
| LOW (-12 dBm) | 0.06 mW | ~30 cm | Near-field test |
| HIGH (-6 dBm) | 0.25 mW | ~2 m | Medium range |
| **MAX (0 dBm)** | **1 mW** *(+20 dB with PA+LNA)* | **~50-100 m** | **Default — maximum disruption** |

> If the OLED corrupts or the ESP32 reboots at MAX, drop to HIGH or LOW.

---

### 2. `Rate:` — Air Data Rate

Controls NRF24 on-air bitrate.

| Value | Bandwidth | Best for |
| :---: | :---: | :--- |
| **250 K** | ±1 MHz | **Maximum energy concentration per channel** — best for narrow targets |
| 1 M | ±1 MHz | Balanced |
| 2 M | ±2 MHz | Wider footprint, faster hop |

> **250 K** is recommended for maximum jamming efficiency on a specific channel.

---

### 3. `Hop:` — Channel Hop Delay

Time in microseconds between channel changes.

| Value | Effect |
| :---: | :--- |
| **0 µs** | Fastest hop — best vs FHSS (Bluetooth, drones) |
| 20 µs | Fast |
| **50 µs** | **Default — balanced** |
| 100 µs | Slow but radios stabilize better on noisy setups |

---

### 4. `Rad:` — Active Radios

Which NRF24 module(s) are enabled.

| Value | Behavior |
| :---: | :--- |
| **Dual** | Both radios active — jam 2 channels simultaneously |
| Radio1 | Only NRF1 (GPIO 20/21) |
| Radio2 | Only NRF2 (GPIO 7/10) |

Use single-radio mode for troubleshooting or if one module is dead.

---

### 5. `Proto:` — Protocol Profile

Fallback strategy when **Auto** and **Preset** are both OFF.

| Value | Range | Use for |
| :---: | :--- | :--- |
| **BT Classic** | NRF 2-80 | Bluetooth speakers, headsets, car audio |
| **BLE** | NRF 2-80 (even) | BLE mice, keyboards, beacons, tags |
| **WiFi** | NRF 12, 37, 62 | WiFi routers (channels 1, 6, 11) |
| **Drone** | NRF 0-124 | FPV drones, RC control |
| **Multi** | NRF 0-125 | Everything (random) |

> Only used when **Auto = OFF**, **Preset = OFF**, and **R1 Ch / R2 Ch = 0**.

---

### 6. `Target:` — Target Device Bias

Weights channel selection in BT/BLE modes.

| Value | Bias strategy |
| :---: | :--- |
| **All** | Uniform across full band |
| **Audio** | Weighted toward BT Classic A2DP audio bands |
| **HID** | Weighted toward BLE advertising (NRF 2, 26, 80) |
| **Beacon** | Weighted toward BLE advertising channels |

> Only used when **Auto = OFF** and **Preset = OFF**.

---

### 7. `Auto:` — Automatic RPD Scanner

| Value | Behavior |
| :---: | :--- |
| **ON** | Scans 0-125 using RPD, then **concentrates jamming on channels with detected energy** |
| OFF | Uses Preset / Manual / Protocol fallback |

> Enabling **Auto** automatically disables **Preset**.
>
> In Auto mode, the device rescans every 500 hops (~25 ms at 50 µs hop delay).

---

### 8. `Preset:` — Preset Library Mode

| Value | Behavior |
| :---: | :--- |
| **ON** | Hop only through the channel list of the selected preset |
| OFF | Uses Auto / Manual / Protocol fallback |

> Enabling **Preset** automatically disables **Auto**.

---

### 9. `> PresetName` — Preset Selection

Scrolls through all 14 presets (see [Preset Library](#-preset-library) below).

> Only active when **Preset = ON**.

---

### 10. `Thr:` — Auto Mode Threshold

Minimum RPD activity score required for a channel to be considered "active" in Auto mode.

| Value | Effect |
| :---: | :--- |
| 10 % | Very sensitive — includes weak signals (noisier) |
| **40 %** | **Default — balanced** |
| 70 % | Selective — only strong signals |
| 90 % | Extreme — only extremely noisy channels |

> Only active when **Auto = ON**.

---

### 11. `R1 Ch:` — Manual Channel for Radio 1

Fixed NRF channel for Radio 1 (0-125), steps of 5.

> Only used when **Auto = OFF**, **Preset = OFF**, and **R1 Ch ≠ 0**.

---

### 12. `R2 Ch:` — Manual Channel for Radio 2

Same as above for Radio 2.

> Set both to specific values for narrow two-channel jamming.

---

### 13. `RxSmp:` — Reactive Samples

RPD samples per channel (1-10). More samples = better detection accuracy, slightly slower sweep.
- **Default:** 3
- Use **5** in noisy areas, **1** for maximum hopping speed.

---

### 14. `RxDwl:` — Reactive Dwell Time

How long the Striker holds a hot channel before falling back to sweeping (5-200 ms).
- **Default:** 20 ms
- Use **50-100 ms** for steady targets, **10 ms** for fast frequency hoppers (FHSS).

---

### 15. `RxSwp:` — Reactive Sweep Mode

Defines which channels the Scout scans in Reactive mode.
- **PROTO (Default):** Scans only the current protocol / preset channel list (much faster, targeted).
- **ALL:** Scans all 126 channels (0-125) (best when target protocol is unknown).

---

### 16. `HybSc:` — Hybrid Rescan Period

Interval in milliseconds (300-5000 ms) at which Radio 2 briefly pauses to scan and lock onto the loudest channel.
- **Default:** 1500 ms

---

### 17. `< Reset Defaults >`

Wipes SPIFFS and restores factory settings. You'll see a confirmation popup.

---

### 18. `[ Save & Exit ]`

Writes current configuration to `/settings.bin` on SPIFFS. **Always press this when done.**

---

### 19. `[ Back / Discard ]`

Reloads settings from SPIFFS — discards unsaved changes and returns to main menu.

---

## 📡 Channel Mapping

The NRF24L01+ channel number **N** corresponds to frequency **2400 + N MHz**.

| Protocol | Frequency Range | NRF Channel Range |
| :--- | :--- | :--- |
| Bluetooth Classic | 2402 – 2480 MHz | **2 – 80** (79 channels) |
| BLE Data (all 40) | 2404 – 2480 MHz | **2, 4, 6, …, 80** (even) |
| BLE Adv 37 | 2402 MHz | **2** |
| BLE Adv 38 | 2426 MHz | **26** |
| BLE Adv 39 | 2480 MHz | **80** |
| WiFi ch 1 | 2412 MHz | **12** |
| WiFi ch 6 | 2437 MHz | **37** |
| WiFi ch 11 | 2462 MHz | **62** |
| WiFi ch 13 | 2472 MHz | **72** |
| Drone band | 2400 – 2525 MHz | **0 – 124** |

---

## 📚 Preset Library

14 pre-tuned channel lists. Selected via **Settings → Preset ON → Preset Sel**.

| # | Preset Name | NRF Channels | Best for |
| :---: | :--- | :--- | :--- |
| 1 | Full Spectrum | 0-125 | Blanket jamming |
| 2 | WiFi 2.4GHz | 12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72 | All 13 WiFi channels |
| 3 | **WiFi Priority** | 12, 37, 62 | Routers on ch 1/6/11 |
| 4 | BLE All Data | 2, 4, …, 80 | Full BLE band |
| 5 | **BLE Adv Pri** | 2, 26, 80 | **Mice, keyboards, BLE beacons** |
| 6 | BLE Adv + Nb | 2, 24, 26, 28, 78, 80 | Adv + close neighbors |
| 7 | BT Classic | 2-80 | Speakers, headsets, car audio |
| 8 | BT Inquiry | 2, 34, 50, 66 | Block device discovery |
| 9 | Drone FHSS | 0, 5, 10, …, 95 | Racing drones |
| 10 | Drone Full | 0-124 | All drone bands |
| 11 | USB Dongle | 40, 50, 60, 70, 80, 90 | Logitech Unifying |
| 12 | FPV Video | 70, 75, 80, 85, 90, 95 | Analog video TX |
| 13 | RC Control | 4, 8, 12, 16 | RC cars, boats, planes |
| 14 | Zigbee | 15, 20, 25, 30 | Zigbee HA networks |

---

## 🤖 Auto Mode (RPD Scanner)

The NRF24L01+ has a **Received Power Detector (RPD)** register that latches HIGH when a signal stronger than **-64 dBm** is present. Cypher Box uses this as a 1-bit energy detector.

**How it works:**

1. Both radios enter RX mode with a noise address.
2. Radio 1 scans channels 0-62, Radio 2 scans 63-125 (in parallel).
3. Each channel is sampled 30 times — activity score = `hits / 30 × 100 %`.
4. A weighted distribution is built from the scores.
5. Jamming hops preferentially to high-activity channels.
6. Rescans every 500 hops.

**Adjust the threshold** via **Settings → Thr**:
- Low threshold = broad, catches weak signals
- High threshold = selective, only strong signals

> **Known limitation:** RPD is binary (signal present / not present), not a true RSSI value. It cannot distinguish between a weak WiFi packet and a strong WiFi packet.

---

## 🎯 New Modes Explained

### How the Reactive Jammer Works

```
┌──────────────────┐    reports hot ch    ┌──────────────────┐
│    RADIO 1       │  ───────────────►   │    RADIO 2       │
│    (Scout)       │                     │    (Striker)     │
│                  │                     │                  │
│  • RX mode       │                     │  • TX const      │
│  • Noise pipe    │                     │    carrier       │
│  • PA=MIN        │                     │  • PA=from cfg   │
│  • Hops fast     │                     │  • Locks onto    │
│  • Tests RPD     │                     │    hot channel   │
└──────────────────┘                     └──────────────────┘
     scans ch N                              jams ch N
     scans ch N+1                            holds N for dwell
     scans ch N+2                            if no report: sweep
```

### Reactive Settings

| Setting | What it does | Best value |
| :--- | :--- | :--- |
| **RxSmp** (1-10) | RPD samples per channel. More samples = better detection, slower scan. | **3** default. Use **5** in noisy areas, **1** for max speed. |
| **RxDwl** (5-200ms) | How long the Striker holds a hot channel before falling back to sweeping. | **20ms** default. Use **50-100ms** for steady targets, **10ms** for FHSS. |
| **RxSwp** (ALL/PROTO) | `ALL` = scan all 126 channels. `PROTO` = scan only the current protocol/preset list. | **PROTO** for focused jamming (faster), **ALL** for unknown targets. |

### Which Channel List Does Reactive Use?

Priority chain (same as blind modes):

```
1. Preset ON?      → uses the selected preset's channel list
2. RxSwp = PROTO?  → uses Proto fallback (BT=79ch, BLE=40ch, WiFi=13ch, Drone=125ch)
3. RxSwp = ALL?    → uses all 126 channels
```

### Recommended Configurations

| Target | Settings |
| :--- | :--- |
| **BT speaker, known nearby** | Preset OFF, Proto=BT Classic, RxSwp=PROTO, RxSmp=3, RxDwl=30ms |
| **BLE mouse/keyboard** | Preset ON, Preset Sel=BLE Adv Pri, RxSmp=5, RxDwl=50ms |
| **WiFi router** | Preset ON, Preset Sel=WiFi Priority, RxSmp=3, RxDwl=100ms |
| **Unknown / mixed** | Preset OFF, RxSwp=ALL, RxSmp=2, RxDwl=15ms |
| **Drone** | Preset ON, Preset Sel=Drone FHSS, RxSmp=3, RxDwl=20ms |

---

### How To Use Reactive Mode

1. Flash `JammerC-3.ino` to the ESP32-C3.
2. On first boot, settings will reset (`0xCB05` magic mismatch → defaults). Go to **Settings → Save & Exit** once.
3. From main menu, navigate to **Reactive JamX** (down from MultiCH).
4. Press **SEL** — it starts scanning + reactive jamming.
5. Press **SEL** again to stop.

#### Behavior You'll Observe:
- The Striker (**Radio 2**) will spend most of its time on channels that the Scout (**Radio 1**) detects as active.
- If nothing is transmitting, the Striker sweeps randomly through the channel list to keep some pressure.
- You won't see the display update during reactive mode (it would defeat the purpose), but you'll notice **huge real-world jamming efficiency** vs blind hopping.

---

### ⚠️ Known Limitations

1. **Radio 1 can only listen on one channel at a time.** If a BT device hops every 625 µs and you scan one channel every ~600 µs, you'll catch ~50–70% of hops. This is still vastly better than blind hopping, which catches the correct channel ~1/79 of the time.
2. **RPD is 1-bit.** It tells you "signal present > -64 dBm" or not. It doesn't tell you how strong. This is a hardware limitation of the NRF24L01+ — you'd need an SDR for true RSSI.
3. **Striker keeps last channel until re-tasked.** If the target stops transmitting, the Striker sweeps randomly after `RxDwl` ms. Fine for keeping pressure up, but not stealthy.
4. **The Scout stops transmitting entirely.** In reactive mode, the Scout never TX's — it's purely a receiver. Only the Striker jams. So you get 1 radio jamming instead of 2, but placed far more intelligently.

---

### 1. Hybrid JamX

**Radio 1 (TX):** blind hops using preset / protocol / auto config — constantly.

**Radio 2 (TX):** starts on channel 12, then every `HybSc` ms (default 1500ms):
1. Pauses its const carrier
2. Briefly switches to RX scout mode
3. Tests 12 channels for RPD activity
4. Locks onto the strongest detected channel
5. Resumes const carrier on that channel

**Both radios jam in parallel the entire time.** While Radio 1 covers the whole configured band, Radio 2 concentrates on the "hottest" active channel.

---

### 2. Channel Picker

The exact workflow:

```
[SCAN]    →  scans 126 channels with both radios
[SELECT]  →  shows top 10 detected channels with activity %
             UP/DOWN navigates the list
             SEL locks your choice
[JAM]     →  BOTH radios lock onto that single channel
             Both transmit const carrier indefinitely
             Only SEL can stop it
```

---

### 📖 How to Use Each Mode

#### Hybrid JamX — "everything on"
1. Optional: **Settings → Preset ON** → pick preset (Radio 1 will use this).
2. Menu → **Hybrid JamX** → **SEL**.
3. Radio 1 hops through the preset, Radio 2 locks onto whatever's loudest.
4. **SEL** to stop.

#### Channel Picker — "manual sniper"
1. Menu → **Channel Picker** → **SEL**.
2. Wait ~5 seconds while it scans.
3. Screen shows detected hot channels:
   ```
   PICK CHANNEL
   NRF 37    84%
   NRF 12    62%
   NRF 62    51%
   NRF  6    18%
   NRF 18    12%
   UP/DN SEL=Lock
   ```
4. **UP/DOWN** to highlight the channel you want.
5. **SEL** → screen shows: `"LOCKED JAM / NRF CH: 37 / Both radios TX"`.
6. Both radios jam channel 37 **forever** until you press **SEL** again.

---

### ⚖️ Mode Comparison & Tradeoffs

| Mode | Radios Jamming | Scanning | Best For |
| :--- | :--- | :--- | :--- |
| **Reactive** | 1 (R2) | Continuous (R1) | Stealth, single narrow target |
| **Hybrid** | 2 (mostly) | Periodic ~200ms pause on R2 | Maximum pressure + adaptation |
| **Channel Picker** | 2 (both) | Once at start | Known static target |
| **Blind** | 2 | None | Unknown / mixed environment |

---

## 🎯 Use Cases & Recipes

### 🎵 Target a Bluetooth Speaker Only

A Bluetooth speaker is a **BT Classic A2DP** device (not BLE).

| Setting | Value |
| :--- | :--- |
| Proto | **BT Classic** |
| Target | **Audio** |
| Auto | OFF |
| Preset | OFF |
| R1 Ch, R2 Ch | 0, 0 |
| PA | MAX |
| Rate | 250 K |
| Hop | 0 |
| Rad | Dual |

Then run **BT/BLE Jammer**.

---

### 🖱️ Target a Wireless Mouse / Keyboard (BLE)

| Setting | Value |
| :--- | :--- |
| Preset | **ON** |
| Preset Sel | **BLE Adv Pri** |
| Rad | Dual |
| PA | MAX |
| Rate | 250 K |
| Hop | 0 |

Then run **BT/BLE Jammer**.

---

### 📶 Target a WiFi Router

| Setting | Value |
| :--- | :--- |
| Preset | **ON** |
| Preset Sel | **WiFi Priority** |

Then run **Wifi Jammer**.

---

### 🚁 Target a Drone

| Setting | Value |
| :--- | :--- |
| Preset | **ON** |
| Preset Sel | **Drone FHSS** |

Then run **Drone Jammer**.

---

### 🌐 Don't Know What's Around

1. Run **Spectrum Scan** first — see the RF landscape.
2. Note the busiest channels.
3. Set **Auto = ON**, **Thr = 40**.
4. Run any jam mode — the device will focus fire on active channels.

---

## 🛠️ Hardware Tips & Troubleshooting

### OLED Corrupts / Freezes During Jamming

This is **RF coupling into the I2C bus**. Fix in order:

1. ✅ Add **4.7 kΩ pull-ups** on SDA and SCL to 3.3 V
2. ✅ Add **100 nF cap** on OLED VCC
3. ✅ Lower **PA** to HIGH or LOW
4. ✅ Shorten I2C wires (< 10 cm)
5. ✅ Add a **shielded enclosure** or ferrite bead on I2C lines

The firmware auto-recovers the I2C bus after every jam session.

---

### ESP32 Reboots When Jamming Starts

**Power supply problem.** The ESP32-C3's onboard regulator can't feed two NRF24 PA+LNA modules.

Fix:
- Use a **dedicated AMS1117-3.3V** regulator for the NRF modules
- Add **47 µF** electrolytic + **100 nF** ceramic on each NRF VCC
- Power the whole device from a **good 5 V / 2 A USB source**

---

### One Radio Not Detected

Check:
1. **CSN pin** — the GPIO must match firmware (`21` for R1, `10` for R2)
2. **Solder joints** on the NRF module
3. **3.3 V** at the module VCC pin
4. Try **Test Radios** from main menu

---

### Upload Fails

- Hold **BOOT (GPIO 9)**, tap **RESET (EN)**, release BOOT → enters download mode
- Check **USB CDC On Boot = Enabled**
- Use a **data-capable USB cable** (not charge-only)

---

### Settings Not Persisting

- Ensure **Save & Exit** is pressed (not **Back / Discard**)
- Check serial output for `Settings saved to SPIFFS`
- If SPIFFS is corrupted, run **Reset Defaults**

---

## 🙏 Credits

- **RF24 library** — TMRh20
- **Adafruit GFX / SSD1306** — Adafruit Industries
- **U8g2_for_Adafruit_GFX** — Oliver Kraus
- **RPD scanning technique** — inspired by the RF24 scanner example, [nRFBox](https://github.com/nRFBox/nRFBox), and Bruce firmware
- **cypher-jammer-mini** — Original Project Idea [dkyazzentwatwa](https://github.com/dkyazzentwatwa/cypher-jammer-mini)

---

## ⚖️ License

MIT License — see `LICENSE` file for details.

**You are solely responsible for how you use this tool. Comply with all local laws.**

---

## 🌟 Contributing

Pull requests welcome. For major changes, open an issue first.

Ideas for future versions:
- Live spectrum display while jamming (one radio scans, other jams)
- SD card logging of detected RF activity
- Bluetooth MAC capture alongside jamming
- Battery voltage monitoring on OLED

---

## 📜 Disclaimer

This project is provided **strictly for educational and authorized testing purposes**. Transmitting on frequencies you do not own or have authorization to operate on is **illegal** in most jurisdictions, including under FCC Part 15 in the United States, ETSI regulations in the EU, and equivalent laws worldwide.

**The author assumes no liability for misuse, damage, or legal consequences.** Use only:
- On hardware you personally own
- Inside an RF-shielded room or Faraday enclosure
- Where local regulations explicitly permit
- With permission from the spectrum license holder

By using this project you accept full responsibility for your actions.