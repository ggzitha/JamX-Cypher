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
    hp->setFrequency(16000000);
    hp->setBitOrder(MSBFIRST);
    hp->setDataMode(SPI_MODE0);
  }
  for ( int i = 0; i < nrf24_count; i++ )
  {
    radios[i]->begin(hp);
    radios[i]->setAutoAck(false);
    radios[i]->stopListening();
    radios[i]->setRetries(0, 0);
    radios[i]->setPayloadSize(5);
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
    hp->setFrequency(16000000);
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
              radios[j]->setChannel(bluetooth_channels[start[j] + i]);
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
          radios[j]->setChannel(random_channel);
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
              radios[j]->setChannel(start[j] + i);
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
            radios[j]->setChannel(bluetooth_channels[ch]);
          }

          delayMicroseconds(jam_delay);
        }
      }
      if ( bluetooth_jam_method == 1 )
      {
        int random_channel = random(80);
        for ( int j = 0; j < nrf24_count; j++ )
        {
          radios[j]->setChannel(random_channel);
        }

        delayMicroseconds(jam_delay);
      }
      if ( bluetooth_jam_method == 2 )
      {
        for ( int ch = 0; ch < 80; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            radios[j]->setChannel(ch);
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
          radios[j]->setChannel(random_channel);
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
              radios[j]->setChannel(start[j] + i);
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
          radios[j]->setChannel(random_channel);
        }

        delayMicroseconds(jam_delay);
      }
      if ( drone_jam_method == 1 )
      {
        for ( int ch = 0; ch < 125; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            radios[j]->setChannel(ch);
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
          radios[j]->setChannel(ble_channels[ch]);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
        }
      }
    }
    else
    {
      for ( int ch = 0; ch < 3; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          radios[j]->setChannel(ble_channels[ch]);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
        }
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
            radios[j]->setChannel(2 + (start[j] + i) * 2);
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
          radios[j]->setChannel(ch);
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
            radios[j]->setChannel(ch);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
          }
        }
      }
      else
      {
        for ( int ch = (channel * 5) + 1; ch <= (channel * 5) + 23; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            radios[j]->setChannel(ch);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
              radios[j]->setChannel(ch);
              radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
              radios[j]->setChannel(ch);
              radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
        }
      }
    }
    else
    {
      for ( int ch = (global_ch1 * 5) + 1; ch <= (global_ch1 * 5) + 23; ch++ )
      {
        for ( int j = 0; j < nrf24_count; j++ )
        {
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(ch);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
          }
        }
      }
      else
      {
        for ( int ch = 4 + 5 * (channel - 11); ch <= (5 + 5 * (channel - 11)) + 2; ch++ )
        {
          for ( int j = 0; j < nrf24_count; j++ )
          {
            radios[j]->setChannel(ch);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(global_ch1 + start[j] + i);
            if ( misc_jam_method == 1 )
            {
              radios[j]->writeFast(&jam_text, sizeof(jam_text));
            }
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
          radios[j]->setChannel(ch);
          if ( misc_jam_method == 1 )
          {
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
          }
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
            radios[j]->setChannel(1 + start[j] + i);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(2 + start[j] + i);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(45 + start[j] + i);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(ch);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(unifying_chs[start[j] + i]);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(unifying_chs[i]);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(2 + start[j] + i);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(start[j] + i);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(flysky_chs[start[j] + i]);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(flysky_chs[i]);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(start[j] + i);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
            radios[j]->setChannel(start[j] + i);
            radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
          radios[j]->setChannel(ch);
          radios[j]->writeFast(&jam_text, sizeof(jam_text));
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
// jamHandler
// ---------------------------------------------------------------------------
bool jamHandler(void (*action)(), String htmlResponse, const unsigned char *bitmap, bool CW, bool mode, bool isMisc, bool scan, int ch1, int ch2)
{
  (void)htmlResponse;

  global_ch1              = ch1;
  global_ch2              = ch2;
  stopJam                 = false;
  TaskHandle_t TaskHandle = NULL;

  if ( !isMisc )
  {
    display.clearDisplay();
    display.drawBitmap(0, 0, bitmap, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
    display.display();
  }

  // Only push the jam HTML page if a real HTTP client is connected.
  // Serial- or button-triggered jams have no active request; calling
  // server.send_P() in that state can block on some ESP32 core versions
  // and prevents the jam task from ever being created.
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

  // The BLE attack tasks use NimBLE + std::string + heap allocation and need
  // a much larger stack than the nRF24 channel-hoppers.  A 4 KB stack causes
  // a silent panic/reboot shortly after start.
  uint32_t stackSize = 4096;
  if ( action == ble_apple_flood_task ||
       action == ble_adv_flood_task ||
       action == ble_scan_flood_task )
  {
    stackSize = 16384;
  }

  xTaskCreatePinnedToCore(reinterpret_cast<TaskFunction_t>(action), "Jammer", stackSize, NULL, 3, &TaskHandle, 0);

  updateJoystickButtons();
  bool SerialStop = true;
  bool exit       = false;

  while ( !exit && SerialStop )
  {
    randomSeed(esp_random() + millis());
    updateJoystickButtons();
    SerialStop = SerialCommands();

    if ( btnOK.isSingle() )
    {
      exit = true;
      break;
    }

    if ( (btnNext.isSingle() || btnPrevious.isSingle() || btnOK.isDouble()) && mode )
      break;
    delay(1);
  }

  stopJam = true;
  while ( eTaskGetState(TaskHandle) != eDeleted )
    delay(1);

  TaskHandle = NULL;

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