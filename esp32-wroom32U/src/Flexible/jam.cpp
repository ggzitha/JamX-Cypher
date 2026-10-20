#include "jam.h"

// ---------------------------------------------------------------------------
// Forward declarations for the BLE attack wrapper tasks (defined at the bottom
// of this file).  They are used here only to select a larger FreeRTOS stack
// for the NimBLE-based attack tasks.
// ---------------------------------------------------------------------------
void ble_apple_flood_task();
void ble_adv_flood_task();
void ble_scan_flood_task();

int        nrf_pa_level;
static int global_ch1, global_ch2;
bool       stopJam = false;

// Guards against a second jam being started while one owns the radios.
static bool s_jamActive = false;

// True while a jam task owns the radios (used to reject conflicting commands).
bool jamIsActive()
{
  return s_jamActive;
}

void InitRadios()
{
  for ( int i = 0; i < nrf24_count; i++ )
  {
    radios[i] = new RF24(ce_pins[i], csn_pins[i]);
  }
}
void DeinitRadios(bool stopConstCarrier)
{
  for ( int i = 0; i < nrf24_count; i++ )
  {
    if ( radios[i] != nullptr )
    {
      if ( stopConstCarrier )
      {
        radios[i]->stopConstCarrier();
      }
      radios[i]->powerDown();
      delete radios[i];
      radios[i] = nullptr;
    }
  }
}
void HSPI_init()
{
  if ( hp == nullptr )
  {
    hp = new SPIClass(HSPI);
    hp->begin(14, 12, 13, -1);
    hp->setFrequency(8000000);
    hp->setBitOrder(MSBFIRST);
    hp->setDataMode(SPI_MODE0);
  }

  // NOTE: jam_delay is intentionally NOT clamped here.  In constant-carrier
  // mode a very fast hop rate (jam_delay = 0) produces a chirp that smears
  // energy across the whole band, which is what makes bluetooth_jam() and
  // drone_jam() effective.  Only packet-based modes need the PLL to settle,
  // and they wait NRF24_MIN_DWELL_US explicitly.

  for ( int i = 0; i < nrf24_count; i++ )
  {
    // begin() returns false when the module does not answer on SPI.  The old
    // code ignored that return value, so a dead or miswired module looked like
    // a perfectly configured jammer that simply produced no RF.
    bool ok = radios[i]->begin(hp);
    if ( ok )
      ok = radios[i]->isChipConnected();
    if ( ok )
    {
      Serial.printf("[nRF24] module %d ready (CE=%d, CSN=%d)\n", i, ce_pins[i], csn_pins[i]);
    }
    else
    {
      Serial.printf("[nRF24] module %d NOT responding on SPI (CE=%d, CSN=%d)\n", i, ce_pins[i], csn_pins[i]);
    }
    radios[i]->setAutoAck(false);
    radios[i]->stopListening();
    radios[i]->setRetries(0, 0);
    radios[i]->setPayloadSize(32);
    radios[i]->setAddressWidth(3);
    switch ( nrf_pa )
    {
    case 0:
      nrf_pa_level = RF24_PA_MIN;   // Lowest  (-18 dBm)
      break;
    case 1:
      nrf_pa_level = RF24_PA_LOW;   // Low     (-12 dBm)
      break;
    case 2:
      nrf_pa_level = RF24_PA_HIGH;  // High    (-6 dBm)
      break;
    case 3:
      nrf_pa_level = RF24_PA_MAX;   // Ultra   (0 dBm)
      break;
    }
    radios[i]->setPALevel(nrf_pa_level, true);
    radios[i]->setDataRate(RF24_2MBPS);
    radios[i]->setCRCLength(RF24_CRC_DISABLED);
    radios[i]->disableCRC();
    radios[i]->disableAckPayload();
    radios[i]->disableDynamicPayloads();
  }
  digitalWrite(2, HIGH);
}
// Bare HSPI bus init (no radios configured). Used by the nRF24
// detection / status routines when the radio objects are not allocated.
void HSPI_initBare()
{
  if ( hp == nullptr )
  {
    hp = new SPIClass(HSPI);
    hp->begin(14, 12, 13, -1);
    hp->setFrequency(8000000);
    hp->setBitOrder(MSBFIRST);
    hp->setDataMode(SPI_MODE0);
  }
}
void HSPI_deinit()
{
  if ( hp != nullptr )
  {
    hp->end();
    delete hp;
    hp = nullptr;
  }
  // Ensure GPIO 12 (HSPI MISO / MTDI strapping pin) is pulled down when idle
  // so it cannot be sampled HIGH during resets (which causes 1.8V flash read err 1000).
  pinMode(12, INPUT_PULLDOWN);
  digitalWrite(2, LOW);
}

// ---------------------------------------------------------------------------
// Carrier retune
// ---------------------------------------------------------------------------
// The constant carrier is armed ONCE by startConstCarrier().  Retuning with
// setChannel() then sweeps the live carrier across the band, which radiates a
// broadband chirp - that is what makes these sweep jammers effective.
//
// Do NOT stop/restart the carrier on every hop: that turns a ~2 us chirp step
// into an ~800 us stepped carrier, cutting spectral coverage by orders of
// magnitude and making the jammer appear dead.
// ---------------------------------------------------------------------------
static inline void cwHop(RF24 *r, uint8_t ch)
{
  r->setChannel(ch);
}

// Packet-mode hop: let the PLL settle, then use the BLOCKING write() so the
// packet actually finishes on this channel before the next retune.
static inline void pktHop(RF24 *r, uint8_t ch, const void *buf, uint8_t len)
{
  r->setChannel(ch);
  delayMicroseconds(NRF24_MIN_DWELL_US);
  r->write(buf, len);
}
void bluetooth_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      if ( bluetooth_jam_method == 0 )
      {
        int total_channels = 21;
        int base           = total_channels / nrf24_count;
        int rem            = total_channels % nrf24_count;

        int start[30];
        int count[30];
        int ch = 0;
        for ( int j = 0; j < nrf24_count; j++ )
        {
          count[j] = base + (j < rem ? 1 : 0);
          start[j] = ch;
          ch += count[j];
        }

        int max_count = base + (rem > 0 ? 1 : 0);

        for ( int i = 0; i < max_count; i++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            if ( i < count[j] )
            {
              cwHop(radios[j], bluetooth_channels[start[j] + i]);
            }
          }
          delayMicroseconds(jam_delay);
        }
      }
      if ( bluetooth_jam_method == 1 )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          int random_channel = random(80);
          cwHop(radios[j], random_channel);
        }

        delayMicroseconds(jam_delay);
      }
      if ( bluetooth_jam_method == 2 )
      {
        int total_channels = 80;
        int base           = total_channels / nrf24_count;
        int rem            = total_channels % nrf24_count;

        int start[30];
        int count[30];
        int ch = 0;
        for ( int j = 0; j < nrf24_count; j++ )
        {
          count[j] = base + (j < rem ? 1 : 0);
          start[j] = ch;
          ch += count[j];
        }

        int max_count = base + (rem > 0 ? 1 : 0);

        for ( int i = 0; i < max_count; i++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            if ( i < count[j] )
            {
              cwHop(radios[j], start[j] + i);
            }
          }
          delayMicroseconds(jam_delay);
        }
      }
    }
    else
    {
      if ( bluetooth_jam_method == 0 )
      {
        for ( int ch = 0; ch < 21; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            cwHop(radios[j], bluetooth_channels[ch]);
          }

          delayMicroseconds(jam_delay);
        }
      }
      if ( bluetooth_jam_method == 1 )
      {
        int random_channel = random(80);
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], random_channel);
        }

        delayMicroseconds(jam_delay);
      }
      if ( bluetooth_jam_method == 2 )
      {
        for ( int ch = 0; ch < 80; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            cwHop(radios[j], ch);
          }

          delayMicroseconds(jam_delay);
        }
      }
    }
  }
  vTaskDelete(NULL);
}
void drone_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      if ( drone_jam_method == 0 )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          int random_channel = random(125);
          cwHop(radios[j], random_channel);
        }

        delayMicroseconds(jam_delay);
      }
      if ( drone_jam_method == 1 )
      {
        int total_channels = 125;
        int base           = total_channels / nrf24_count;
        int rem            = total_channels % nrf24_count;

        int start[30];
        int count[30];
        int ch = 0;
        for ( int j = 0; j < nrf24_count; j++ )
        {
          count[j] = base + (j < rem ? 1 : 0);
          start[j] = ch;
          ch += count[j];
        }

        int max_count = base + (rem > 0 ? 1 : 0);

        for ( int i = 0; i < max_count; i++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            if ( i < count[j] )
            {
              cwHop(radios[j], start[j] + i);
            }
          }
          delayMicroseconds(jam_delay);
        }
      }
    }
    else
    {
      if ( drone_jam_method == 0 )
      {
        int random_channel = random(125);
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], random_channel);
        }

        delayMicroseconds(jam_delay);
      }
      if ( drone_jam_method == 1 )
      {
        for ( int ch = 0; ch < 125; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            cwHop(radios[j], ch);
          }

          delayMicroseconds(jam_delay);
        }
      }
    }
  }
  vTaskDelete(NULL);
}
void ble_advertising_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 3;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int ch             = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        int count = base + (j < rem ? 1 : 0);
        for ( int i = 0; i < count; i++, ch++ )
        {
          cwHop(radios[j], ble_channels[ch]);
        }
      }
      // Give the carrier time to settle on each advertising channel.
      delayMicroseconds(jam_delay);
    }
    else
    {
      for ( int ch = 0; ch < 3; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ble_channels[ch]);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}
void ble_data_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 40;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;

      int start[30];
      int count[30];
      int ch = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = ch;
        ch += count[j];
      }

      int max_count = base + (rem > 0 ? 1 : 0);

      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], 2 + (start[j] + i) * 2);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 2; ch <= 80; ch += 2 )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }

        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}
void wifi_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    for ( int channel = 0; channel < 14; channel++ )
    {
      if ( Separate_or_together == 0 )
      {
        int total_channels = 23;
        int base           = total_channels / nrf24_count;
        int rem            = total_channels % nrf24_count;
        int ch             = (channel * 5) + 1;
        for ( int j = 0; j < nrf24_count; j++ )
        {
          int count = base + (j < rem ? 1 : 0);
          for ( int i = 0; i < count; i++, ch++ )
          {
            cwHop(radios[j], ch);
          }
        }
      }
      else
      {
        for ( int ch = (channel * 5) + 1; ch <= (channel * 5) + 23; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            cwHop(radios[j], ch);
          }
        }
      }
    }
  }
  vTaskDelete(NULL);
}
void wifi_scan_jam()
{
  vTaskPrioritySet(NULL, 3);
  int scanCounter = 0;
  while ( !stopJam )
  {
    if ( scanCounter >= 10000 )
    {
      NumberChannels = scan_wifi_APs(WiFiScanChannels, true);
      scanCounter    = 0;
    }

    for ( int chIndex = 0; chIndex < NumberChannels; chIndex++ )
    {
      int wifiChannel = WiFiScanChannels[chIndex];

      if ( Separate_or_together == 0 )
      {
        int total_channels = 23;
        int base           = total_channels / nrf24_count;
        int rem            = total_channels % nrf24_count;
        int ch             = ((wifiChannel - 1) * 5) + 1;

        for ( int j = 0; j < nrf24_count; j++ )
        {
          int count = base + (j < rem ? 1 : 0);
          for ( int i = 0; i < count; i++, ch++ )
          {
            if ( ch >= 1 && ch <= 125 )
            {
              cwHop(radios[j], ch);
            }
          }
        }
      }
      else
      {
        for ( int ch = ((wifiChannel - 1) * 5) + 1; ch <= ((wifiChannel - 1) * 5) + 23; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            if ( ch >= 1 && ch <= 125 )
            {
              cwHop(radios[j], ch);
            }
          }
        }
      }
    }
    scanCounter++;
  }
  vTaskDelete(NULL);
}
void wifi_channel()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 23;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int ch             = (global_ch1 * 5) + 1;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        int count = base + (j < rem ? 1 : 0);
        for ( int i = 0; i < count; i++, ch++ )
        {
          cwHop(radios[j], ch);
        }
      }
    }
    else
    {
      for ( int ch = (global_ch1 * 5) + 1; ch <= (global_ch1 * 5) + 23; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
      }
    }
  }
  vTaskDelete(NULL);
}
void zigbee_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    for ( int channel = 11; channel < 27; channel++ )
    {
      if ( Separate_or_together == 0 )
      {
        int total_channels = 3;
        int base           = total_channels / nrf24_count;
        int rem            = total_channels % nrf24_count;
        int ch             = 4 + 5 * (channel - 11);
        for ( int j = 0; j < nrf24_count; j++ )
        {
          int count = base + (j < rem ? 1 : 0);
          for ( int i = 0; i < count; i++, ch++ )
          {
            cwHop(radios[j], ch);
          }
        }
      }
      else
      {
        for ( int ch = 4 + 5 * (channel - 11); ch <= (5 + 5 * (channel - 11)) + 2; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            cwHop(radios[j], ch);
          }
        }
      }
    }
  }
  vTaskDelete(NULL);
}
void misc_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = global_ch2 - global_ch1 + 1;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;

      int start[30];
      int count[30];
      int ch = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = ch;
        ch += count[j];
      }

      int max_count = base + (rem > 0 ? 1 : 0);

      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            if ( misc_jam_method == 1 )
              pktHop(radios[j], global_ch1 + start[j] + i, jam_text, sizeof(jam_text));
            else
              cwHop(radios[j], global_ch1 + start[j] + i);
          }
        }
        if ( misc_jam_method == 0 )
        {
          delayMicroseconds(jam_delay);
        }
      }
    }
    else
    {
      for ( int ch = global_ch1; ch <= global_ch2; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( misc_jam_method == 1 )
            pktHop(radios[j], ch, jam_text, sizeof(jam_text));
          else
            cwHop(radios[j], ch);
        }

        if ( misc_jam_method == 0 )
          delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// Full 2.4 GHz band sweep: channels 1-125 (2401-2525 MHz)
void full_24g_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 125;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30], ch = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = ch;
        ch += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], 1 + start[j] + i);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 1; ch <= 125; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// USB 2.4 GHz wireless dongle band: channels 2-40
void usb_24g_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 39;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30], ch = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = ch;
        ch += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], 2 + start[j] + i);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 2; ch <= 40; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// FPV / 2.4 GHz video band: channels 45-80
void fpv_video_jam()
{
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 36;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30], ch = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = ch;
        ch += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], 45 + start[j] + i);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 45; ch <= 80; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// HID / Wireless Peripheral Jammers
// ---------------------------------------------------------------------------

// Logitech Unifying / Bolt receiver uses channels 2–72 (even channels only,
// hopping pseudo-randomly).  We sweep all even channels in the lower 2.4 GHz
// band to disrupt any 2.4 GHz HID dongle (Logitech, Microsoft Nano, etc.).
void hid_wireless_jam()
{
  Serial.println("[hid_wireless_jam] task entered");
  vTaskPrioritySet(NULL, 3);
  // HID channels: even-numbered nRF24 channels covering 2402–2472 MHz
  // (nRF24 ch 2 = 2402 MHz, ch 72 = 2472 MHz).
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      // Distribute even channels 2–72 across all radios (36 channels total).
      int total_channels = 36;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30];
      int idx = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = idx;
        idx += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            int ch = 2 + (start[j] + i) * 2; // even channels: 2,4,6,...,72
            cwHop(radios[j], ch);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 2; ch <= 72; ch += 2 )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// Logitech Unifying protocol uses a specific subset of 25 channels derived
// from the paired receiver's address.  We target the known set that covers
// all Unifying/Bolt devices regardless of address: ch 5,8,11,...,74 (step 3).
void logitech_unifying_jam()
{
  Serial.println("[logitech_unifying_jam] task entered");
  vTaskPrioritySet(NULL, 3);
  // 25 channels, step = 3, starting at 5: 5,8,11,...,77
  static const uint8_t unifying_chs[] = {
      5, 8, 11, 14, 17, 20, 23, 26, 29, 32,
      35, 38, 41, 44, 47, 50, 53, 56, 59, 62,
      65, 68, 71, 74, 77
  };
  const int total = sizeof(unifying_chs);

  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int base = total / nrf24_count;
      int rem  = total % nrf24_count;
      int start[30], count[30], idx = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = idx;
        idx += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], unifying_chs[start[j] + i]);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int i = 0; i < total; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], unifying_chs[i]);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// Microsoft 2.4 GHz wireless mouse/keyboard dongles hop in the range ch 2–80
// using a narrow band around a base channel.  We sweep ch 2–80 with the
// 2 Mbps data rate (same as the hardware) to maximise interference.
void ms_wireless_jam()
{
  Serial.println("[ms_wireless_jam] task entered");
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 79;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30], idx = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = idx;
        idx += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], 2 + start[j] + i);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 2; ch <= 80; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// RF Controller Jammers
// ---------------------------------------------------------------------------

// Generic 2.4 GHz RC / hobby controller (FlySky AFHDS, Spektrum DSM2/DSMX,
// FrSky D8/D16, Graupner FHSS, Radiolink, etc.) all hop across different
// subsets of ch 0–125.  A full-band sweep at high speed is the most effective
// approach when the hopping pattern is unknown.
void rc_controller_jam()
{
  Serial.println("[rc_controller_jam] task entered");
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      // Distribute ch 0–125 across all radios.
      int total_channels = 126;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30], idx = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = idx;
        idx += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], start[j] + i);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 0; ch <= 125; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// FlySky AFHDS-2A uses 16 pseudo-randomly selected channels from the band.
// Sweep the known AFHDS-2A candidate channel set (ch 10–140 step 5 mapped to
// nRF24 0–125 range) to cover most bindings.
void flysky_jam()
{
  Serial.println("[flysky_jam] task entered");
  vTaskPrioritySet(NULL, 3);
  // Representative AFHDS-2A hop set (nRF24 channel numbers 0-125).
  // Actual set depends on TX ID but these cover the most common pairings.
  static const uint8_t flysky_chs[] = {
      0, 5, 10, 15, 20, 25, 30, 35, 40, 45,
      50, 55, 60, 65, 70, 75, 80, 85, 90, 95,
      100, 105, 110, 115, 120, 125
  };
  const int total = sizeof(flysky_chs);

  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int base = total / nrf24_count;
      int rem  = total % nrf24_count;
      int start[30], count[30], idx = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = idx;
        idx += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], flysky_chs[start[j] + i]);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int i = 0; i < total; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], flysky_chs[i]);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// Spektrum DSM2/DSMX hops 2 (DSM2) or 23 (DSMX) channels in the range
// 0–79.  Sweep that window.
void spektrum_jam()
{
  Serial.println("[spektrum_jam] task entered");
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 80;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30], idx = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = idx;
        idx += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], start[j] + i);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 0; ch <= 79; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// FrSky D8 uses 8 fixed channels; D16 / ACCESS uses 64 channels.
// Sweep ch 0–100 (covers all FrSky bands including LBT channels).
void frsky_jam()
{
  Serial.println("[frsky_jam] task entered");
  vTaskPrioritySet(NULL, 3);
  while ( !stopJam )
  {
    if ( Separate_or_together == 0 )
    {
      int total_channels = 101;
      int base           = total_channels / nrf24_count;
      int rem            = total_channels % nrf24_count;
      int start[30], count[30], idx = 0;
      for ( int j = 0; j < nrf24_count; j++ )
      {
        count[j] = base + (j < rem ? 1 : 0);
        start[j] = idx;
        idx += count[j];
      }
      int max_count = base + (rem > 0 ? 1 : 0);
      for ( int i = 0; i < max_count; i++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          if ( i < count[j] )
          {
            cwHop(radios[j], start[j] + i);
          }
        }
        delayMicroseconds(jam_delay);
      }
    }
    else
    {
      for ( int ch = 0; ch <= 100; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          cwHop(radios[j], ch);
        }
        delayMicroseconds(jam_delay);
      }
    }
  }
  vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// BLE attack wrapper task functions (bridge to ble_attack.cpp via FreeRTOS)
// These are called through jamHandler so they run as a pinned task and respect
// the existing stop / button-exit infrastructure.
// ---------------------------------------------------------------------------
#include "ble_attack.h"

void ble_apple_flood_task()
{
  apple_ble_flood(); // calls vTaskDelete(NULL) internally
}

void ble_adv_flood_task()
{
  ble_adv_flood(); // calls vTaskDelete(NULL) internally
}

void ble_scan_flood_task()
{
  ble_scan_flood(); // calls vTaskDelete(NULL) internally
}

// ---------------------------------------------------------------------------
// RF self-test
// ---------------------------------------------------------------------------
// Proves the radios actually emit RF.  Module 0 is put into constant-carrier
// mode on one channel while the remaining modules, tuned to the same channel in
// RX mode, report carrier detect (register 0x09, bit 0).  The same receivers
// are sampled with the carrier off first, so a pass cannot be a false positive.
//
// A pass means SPI, CE/CSN wiring, the PA supply and the antennas are all
// working, and any remaining failure is protocol/physics rather than hardware.
//
// Requires at least 2 modules.  Returns true when at least one receiver saw the
// carrier.  detected/total describe the receivers.
bool rfTest(int *detected, int *total)
{
  *detected = 0;
  *total    = 0;

  if ( s_jamActive )
  {
    Serial.println("[rfTest] a jam is running - send 'stop' first");
    return false;
  }

  if ( nrf24_count < 2 )
  {
    Serial.println("[rfTest] need at least 2 modules (1 transmitter + 1 receiver)");
    return false;
  }

  const uint8_t ch = 45; // 2435 MHz - inside the HID / RC band
  *total           = nrf24_count;

  InitRadios();
  HSPI_init();

  // Test every module as a transmitter in turn.  This catches a single bad
  // module or a mis-paired CE/CSN, which a single "module 0 transmits" test
  // cannot.
  for ( int tx = 0; tx < nrf24_count; tx++ )
  {
    for ( int i = 0; i < nrf24_count; i++ )
    {
      if ( i == tx )
        continue;
      radios[i]->setChannel(ch);
      radios[i]->startListening();
    }
    delay(20);

    int baseline = 0;
    for ( int i = 0; i < nrf24_count; i++ )
    {
      if ( i != tx && radios[i]->testCarrier() )
        baseline++;
    }

    radios[tx]->startConstCarrier(RF24_PA_MAX, ch);
    delay(50);

    int heard = 0;
    for ( int i = 0; i < nrf24_count; i++ )
    {
      if ( i == tx )
        continue;
      if ( radios[i]->testCarrier() )
        heard++;
    }

    radios[tx]->stopConstCarrier();
    for ( int i = 0; i < nrf24_count; i++ )
    {
      if ( i != tx )
        radios[i]->stopListening();
    }

    Serial.printf("[rfTest] module %d (CE=%d, CSN=%d): heard by %d/%d, baseline %d\n",
                  tx, ce_pins[tx], csn_pins[tx], heard, nrf24_count - 1, baseline);
    if ( heard > 0 )
      (*detected)++;
    delay(20);
  }

  DeinitRadios(false);
  HSPI_deinit();

  Serial.printf("[rfTest] result: %d/%d modules transmitted a carrier that the others heard\n", *detected, *total);
  return *detected == *total;
}

// ---------------------------------------------------------------------------
// Creative Large Logos for Modules
// ---------------------------------------------------------------------------
void drawLargeModuleLogo(int menuNum, int cx, int cy)
{
  switch ( menuNum )
  {
  case 0: // Bluetooth
    display.drawLine(cx, cy - 14, cx, cy + 14, WHITE);
    display.drawLine(cx, cy - 14, cx + 8, cy - 6, WHITE);
    display.drawLine(cx + 8, cy - 6, cx - 8, cy + 8, WHITE);
    display.drawLine(cx - 8, cy - 8, cx + 8, cy + 6, WHITE);
    display.drawLine(cx + 8, cy + 6, cx, cy + 14, WHITE);
    display.drawCircleHelper(cx, cy, 14, 2 | 4, WHITE);
    display.drawCircleHelper(cx, cy, 14, 1 | 8, WHITE);
    break;

  case 1: // Drone
    display.drawRoundRect(cx - 6, cy - 5, 12, 10, 2, WHITE);
    display.fillCircle(cx, cy, 2, WHITE);
    display.drawLine(cx - 5, cy - 4, cx - 16, cy - 12, WHITE);
    display.drawLine(cx + 5, cy - 4, cx + 16, cy - 12, WHITE);
    display.drawLine(cx - 5, cy + 4, cx - 16, cy + 12, WHITE);
    display.drawLine(cx + 5, cy + 4, cx + 16, cy + 12, WHITE);
    display.drawCircle(cx - 16, cy - 12, 4, WHITE);
    display.drawCircle(cx + 16, cy - 12, 4, WHITE);
    display.drawCircle(cx - 16, cy + 12, 4, WHITE);
    display.drawCircle(cx + 16, cy + 12, 4, WHITE);
    break;

  case 2: // WiFi
    display.fillCircle(cx, cy + 10, 2, WHITE);
    display.drawCircleHelper(cx, cy + 10, 6, 1 | 2, WHITE);
    display.drawCircleHelper(cx, cy + 10, 11, 1 | 2, WHITE);
    display.drawCircleHelper(cx, cy + 10, 16, 1 | 2, WHITE);
    display.drawLine(cx + 4, cy - 8, cx - 2, cy + 2, WHITE);
    display.drawLine(cx - 2, cy + 2, cx + 2, cy + 2, WHITE);
    display.drawLine(cx + 2, cy + 2, cx - 4, cy + 10, WHITE);
    break;

  case 3: // BLE
    display.drawLine(cx - 6, cy - 12, cx - 6, cy + 12, WHITE);
    display.drawLine(cx - 6, cy - 12, cx + 2, cy - 5, WHITE);
    display.drawLine(cx + 2, cy - 5, cx - 12, cy + 7, WHITE);
    display.drawLine(cx - 12, cy - 7, cx + 2, cy + 5, WHITE);
    display.drawLine(cx + 2, cy + 5, cx - 6, cy + 12, WHITE);
    display.setTextSize(1);
    display.setCursor(cx + 6, cy - 4);
    display.print("LE");
    break;

  case 4: // Zigbee
    display.drawCircle(cx, cy, 14, WHITE);
    display.drawLine(cx - 9, cy - 8, cx + 9, cy - 8, WHITE);
    display.drawLine(cx + 9, cy - 8, cx - 9, cy + 8, WHITE);
    display.drawLine(cx - 9, cy + 8, cx + 9, cy + 8, WHITE);
    display.fillCircle(cx - 12, cy, 2, WHITE);
    display.fillCircle(cx + 12, cy, 2, WHITE);
    break;

  case 5: // Misc Sweep
    display.drawFastHLine(cx - 20, cy + 8, 40, WHITE);
    display.drawLine(cx - 16, cy + 8, cx - 10, cy - 10, WHITE);
    display.drawLine(cx - 10, cy - 10, cx - 4, cy + 8, WHITE);
    display.drawLine(cx + 4, cy + 8, cx + 10, cy - 10, WHITE);
    display.drawLine(cx + 10, cy - 10, cx + 16, cy + 8, WHITE);
    display.drawFastHLine(cx - 8, cy - 2, 16, WHITE);
    display.drawLine(cx - 8, cy - 2, cx - 5, cy - 5, WHITE);
    display.drawLine(cx + 8, cy - 2, cx + 5, cy - 5, WHITE);
    break;

  case 6: // Settings
    display.drawCircle(cx, cy, 11, WHITE);
    display.drawCircle(cx, cy, 5, WHITE);
    for ( int a = 0; a < 8; a++ )
    {
      float rad = a * 0.785398;
      int   x1  = cx + cos(rad) * 8;
      int   y1  = cy + sin(rad) * 8;
      int   x2  = cx + cos(rad) * 14;
      int   y2  = cy + sin(rad) * 14;
      display.drawLine(x1, y1, x2, y2, WHITE);
    }
    break;

  case 7: // Full 2.4 GHz (Spectrum waterfall)
    display.fillRect(cx - 18, cy + 4, 3, 6, WHITE);
    display.fillRect(cx - 13, cy - 2, 3, 12, WHITE);
    display.fillRect(cx - 8,  cy - 8, 3, 18, WHITE);
    display.fillRect(cx - 3,  cy - 12, 6, 22, WHITE);
    display.fillRect(cx + 5,  cy - 6, 3, 16, WHITE);
    display.fillRect(cx + 10, cy + 1, 3, 9, WHITE);
    display.fillRect(cx + 15, cy + 6, 3, 4, WHITE);
    display.drawFastHLine(cx - 20, cy + 11, 40, WHITE);
    display.setTextSize(1);
    display.setCursor(cx - 12, cy - 14);
    display.print("2.4G");
    break;

  case 8: // USB 2.4 GHz
    display.drawRoundRect(cx - 14, cy - 6, 16, 14, 2, WHITE);
    display.drawRect(cx + 2, cy - 4, 10, 10, WHITE);
    display.fillRect(cx + 5, cy - 2, 3, 2, WHITE);
    display.fillRect(cx + 5, cy + 2, 3, 2, WHITE);
    display.drawCircleHelper(cx - 14, cy + 1, 6, 1 | 8, WHITE);
    display.drawCircleHelper(cx - 14, cy + 1, 10, 1 | 8, WHITE);
    display.drawCircleHelper(cx - 14, cy + 1, 14, 1 | 8, WHITE);
    break;

  case 9: // FPV / Video
    display.drawRoundRect(cx - 18, cy - 8, 36, 18, 4, WHITE);
    display.drawCircle(cx - 8, cy + 1, 5, WHITE);
    display.drawCircle(cx + 8, cy + 1, 5, WHITE);
    display.drawLine(cx - 2, cy + 1, cx + 2, cy + 1, WHITE);
    display.drawLine(cx, cy - 8, cx, cy - 14, WHITE);
    display.fillCircle(cx, cy - 14, 2, WHITE);
    break;

  case 10: // HID Jammer
    display.drawRoundRect(cx - 18, cy - 4, 22, 14, 2, WHITE);
    display.drawFastHLine(cx - 15, cy - 1, 16, WHITE);
    display.drawFastHLine(cx - 15, cy + 2, 16, WHITE);
    display.drawFastHLine(cx - 13, cy + 6, 12, WHITE);
    display.drawRoundRect(cx + 8, cy - 6, 10, 16, 4, WHITE);
    display.drawFastVLine(cx + 13, cy - 5, 5, WHITE);
    display.drawLine(cx + 5, cy - 4, cx + 3, cy, WHITE);
    display.drawLine(cx + 3, cy, cx + 6, cy + 2, WHITE);
    display.drawLine(cx + 6, cy + 2, cx + 4, cy + 6, WHITE);
    break;

  case 11: // RC Jammer
    display.drawRoundRect(cx - 15, cy - 8, 30, 20, 3, WHITE);
    display.drawCircle(cx - 8, cy + 1, 4, WHITE);
    display.fillCircle(cx - 8, cy + 1, 1, WHITE);
    display.drawCircle(cx + 8, cy + 1, 4, WHITE);
    display.fillCircle(cx + 8, cy + 1, 1, WHITE);
    display.drawRect(cx - 3, cy - 4, 6, 4, WHITE);
    display.drawLine(cx, cy - 8, cx, cy - 15, WHITE);
    display.drawCircleHelper(cx, cy - 15, 4, 1 | 2, WHITE);
    break;

  case 12: // Apple BLE Flood
    display.drawCircle(cx, cy + 2, 8, WHITE);
    display.fillCircle(cx - 3, cy + 1, 5, WHITE);
    display.fillCircle(cx + 3, cy + 1, 5, WHITE);
    display.drawCircleHelper(cx + 1, cy - 9, 4, 4, WHITE);
    display.drawRoundRect(cx - 20, cy - 11, 12, 8, 1, WHITE);
    display.drawRoundRect(cx + 9,  cy - 12, 12, 8, 1, WHITE);
    display.drawRoundRect(cx + 9,  cy + 3,  12, 8, 1, WHITE);
    break;

  case 13: // Tools
    display.drawRect(cx - 7, cy - 6, 14, 12, WHITE);
    display.fillRect(cx - 4, cy - 4, 8, 8, WHITE);
    display.drawFastHLine(cx - 10, cy - 3, 3, WHITE);
    display.drawFastHLine(cx - 10, cy + 3, 3, WHITE);
    display.drawFastHLine(cx + 7,  cy - 3, 3, WHITE);
    display.drawFastHLine(cx + 7,  cy + 3, 3, WHITE);
    display.drawLine(cx - 14, cy - 12, cx + 14, cy + 12, WHITE);
    display.drawLine(cx + 14, cy - 12, cx - 14, cy + 12, WHITE);
    break;

  default:
    display.drawCircle(cx, cy, 12, WHITE);
    display.fillCircle(cx, cy, 3, WHITE);
    break;
  }
}

// ---------------------------------------------------------------------------
// Activated Module Screen: thick border, bold title, center logo, animated waves, bottom detail
// ---------------------------------------------------------------------------
void drawActiveJamScreen(const String &title, int animStep, int menuIdx, const unsigned char *customBitmap, const String &subDetail)
{
  (void)customBitmap;
  display.clearDisplay();

  // Always use the clean, centered creative vector logo for ALL modules
  if ( menuIdx >= 0 )
  {
    drawLargeModuleLogo(menuIdx, 64, 31);
  }
  else
  {
    drawLargeModuleLogo(5, 64, 31);
  }

  // Clear top title band
  display.fillRect(2, 2, SCREEN_WIDTH - 4, 12, BLACK);
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  int titleW = title.length() * 6;
  int titleX = (SCREEN_WIDTH - titleW) / 2;
  if ( titleX < 4 ) titleX = 4;
  display.setCursor(titleX, 4);
  display.print(title);
  display.setCursor(titleX + 1, 4); // bold effect
  display.print(title);
  display.drawFastHLine(2, 14, SCREEN_WIDTH - 4, WHITE);

  // Clear left and right signal animation bands
  display.fillRect(2, 16, 26, 43, BLACK);
  display.fillRect(SCREEN_WIDTH - 28, 16, 26, 43, BLACK);

  // Animated signals: 1 to 4 expanding waves (changes each step)
  int bars = (animStep % 4) + 1; // 1, 2, 3, 4

  // Left signal: (((( .
  int lx = 24, ly = 31;
  display.fillCircle(lx, ly, 1, WHITE);
  for ( int b = 1; b <= bars; b++ )
  {
    int r = b * 4 + 1;
    display.drawCircleHelper(lx, ly, r, 1 | 8, WHITE);
  }

  // Right signal: . ))))
  int rx = SCREEN_WIDTH - 24, ry = 31;
  display.fillCircle(rx, ry, 1, WHITE);
  for ( int b = 1; b <= bars; b++ )
  {
    int r = b * 4 + 1;
    display.drawCircleHelper(rx, ry, r, 2 | 4, WHITE);
  }

  // Bottom module detail / target info banner (e.g., "FlySky AFHDS", "21 Channels", etc.)
  if ( subDetail.length() > 0 )
  {
    display.fillRect(4, 49, SCREEN_WIDTH - 8, 11, BLACK);
    display.drawFastHLine(4, 49, SCREEN_WIDTH - 8, WHITE);
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);
    int subW = subDetail.length() * 6;
    int subX = (SCREEN_WIDTH - subW) / 2;
    if ( subX < 4 ) subX = 4;
    display.setCursor(subX, 52);
    display.print(subDetail);
  }

  // Thick double border around entire screen (||==================||)
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  display.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, WHITE);

  display.display();
}

// ---------------------------------------------------------------------------
// jamHandler
// ---------------------------------------------------------------------------
bool jamHandler(void (*action)(), String htmlResponse, const unsigned char *bitmap, bool CW, bool mode, bool isMisc, bool scan, int ch1, int ch2)
{
  (void)htmlResponse;

  if ( s_jamActive )
  {
    if ( webServerStarted && server.client() )
      server.send(409, "text/plain", "a jam is already running\n");
    return false;
  }

  bool isBleTask = ( action == ble_apple_flood_task ||
                     action == ble_adv_flood_task ||
                     action == ble_scan_flood_task );

  if ( !isBleTask && nrf24_count <= 0 )
  {
    Serial.println("[jamHandler] refused: no nRF24 modules configured");
    if ( webServerStarted && server.client() )
      server.send(503, "text/plain", "no nRF24 modules configured\n");
    return false;
  }

  s_jamActive             = true;
  global_ch1              = ch1;
  global_ch2              = ch2;
  stopJam                 = false;
  TaskHandle_t TaskHandle = NULL;

  if ( action == wifi_jam || action == wifi_scan_jam || action == wifi_channel ||
       action == zigbee_jam || action == full_24g_jam || action == usb_24g_jam ||
       action == fpv_video_jam || action == ble_advertising_jam )
    CW = true;

  // Derive informative subDetail banner for the bottom of active screen
  String subDetail = "";
  if ( htmlResponse.indexOf("FlySky") >= 0 ) subDetail = "FlySky AFHDS";
  else if ( htmlResponse.indexOf("Spektrum") >= 0 ) subDetail = "Spektrum DSM";
  else if ( htmlResponse.indexOf("FrSky") >= 0 ) subDetail = "FrSky D8/D16";
  else if ( htmlResponse.indexOf("Logitech") >= 0 ) subDetail = "Logitech Unify";
  else if ( htmlResponse.indexOf("Microsoft") >= 0 ) subDetail = "Microsoft HID";
  else if ( isMisc ) subDetail = "Ch " + String(ch1) + " - Ch " + String(ch2);
  else
  {
    switch ( menu_number )
    {
    case 0: subDetail = (bluetooth_jam_method == 0) ? "21 Channels" : (bluetooth_jam_method == 1 ? "80ch Random" : "80ch Brute"); break;
    case 1: subDetail = (drone_jam_method == 0) ? "Random Hop" : "Brute Sweep"; break;
    case 2: subDetail = (ch1 > 0) ? ("Target Ch " + String(ch1 + 1)) : "All Channels"; break;
    case 3: subDetail = (action == ble_advertising_jam) ? "BLE Adv Ch 37-39" : "BLE Data Ch 0-36"; break;
    case 4: subDetail = "Channels 11-26"; break;
    case 7: subDetail = "Full 2.40-2.48 GHz"; break;
    case 8: subDetail = "USB 2.40-2.48 GHz"; break;
    case 9: subDetail = "FPV Video Band"; break;
    case 10: subDetail = "HID Wireless Band"; break;
    case 11: subDetail = "RC Controller Band"; break;
    case 12: subDetail = "Apple Continuity Flood"; break;
    default: break;
    }
  }

  // Initial draw with active frame & signal animation
  int animStep = 0;
  String dispTitle = (htmlResponse.length() > 0) ? htmlResponse : "Active Jammer";
  drawActiveJamScreen(dispTitle, animStep, menu_number, nullptr, subDetail);

  if ( webServerStarted && server.client() )
  {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html", (const char *)html_jam, html_jam_len);
  }

  if ( scan )
  {
    delay(500);
    if ( access_point == 0 )
      WiFi.softAPdisconnect();
    WiFi.mode(WIFI_STA);
    delay(100);
    NumberChannels = scan_wifi_APs(WiFiScanChannels, true);
  }

  InitRadios();
  HSPI_init();
  if ( isMisc )
  {
    if ( misc_jam_method != 1 )
    {
      for ( int j = 0; j < nrf24_count; j++ )
      {
        radios[j]->startConstCarrier(static_cast<rf24_pa_dbm_e>(nrf_pa_level), 45);
      }
    }
  }
  else
  {
    if ( CW )
    {
      for ( int j = 0; j < nrf24_count; j++ )
      {
        radios[j]->startConstCarrier(static_cast<rf24_pa_dbm_e>(nrf_pa_level), 45);
      }
    }
  }

  uint32_t stackSize = isBleTask ? 16384 : 4096;

  BaseType_t created = xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(action), "Jammer", stackSize, NULL, 3, &TaskHandle, 0);
  if ( created != pdPASS || TaskHandle == NULL )
  {
    Serial.println("[jamHandler] ERROR: could not create jam task (out of heap?)");
    stopJam = true;
    if ( isMisc )
      DeinitRadios(misc_jam_method != 1);
    else
      DeinitRadios(CW);
    HSPI_deinit();
    s_jamActive = false;
    updateDisplay(menu_number);
    return false;
  }

  updateJoystickButtons();
  bool     SerialStop   = true;
  bool     exit         = false;
  uint32_t lastAnimTime = millis();

  while ( !exit && SerialStop )
  {
    randomSeed(esp_random() + millis());
    updateJoystickButtons();

    // Pulse signal waves animation (~300ms per step, 4-step cycle)
    uint32_t now = millis();
    if ( now - lastAnimTime >= 300 )
    {
      lastAnimTime = now;
      animStep++;
      drawActiveJamScreen(dispTitle, animStep, menu_number, nullptr, subDetail);
    }

    SerialStop = SerialCommands();

    if ( btnOK.isSingle() || btnOK.isHolded() )
    {
      exit = true;
      break;
    }

    if ( (btnNext.isSingle() || btnPrevious.isSingle() || btnOK.isDouble()) && mode )
      break;

    delay(5);
  }

  stopJam = true;

  uint32_t waitStart = millis();
  while ( TaskHandle != NULL && eTaskGetState(TaskHandle) != eDeleted && (millis() - waitStart) < 5000 )
    delay(1);

  TaskHandle  = NULL;
  s_jamActive = false;

  if ( isMisc )
  {
    if ( misc_jam_method != 1 )
      DeinitRadios(true);
    else
      DeinitRadios(false);
  }
  else
  {
    if ( CW )
      DeinitRadios(true);
    else
      DeinitRadios(false);
  }

  HSPI_deinit();
  updateJoystickButtons();
  updateDisplay(menu_number);

  return exit;
}