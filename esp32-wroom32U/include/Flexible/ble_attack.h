#ifndef BLE_ATTACK_H
#define BLE_ATTACK_H

// ble_attack.h  — BLE-layer attack declarations
// Requires: NimBLEDevice (already in config.h via NimBLEDevice.h)

#include "config.h"
#include "serial.h"

/// Apple BLE Continuity protocol flood.
/// Cycles through AirDrop / AirPods / Nearby / Handoff opcodes at maximum
/// advertising rate, triggering pop-ups on nearby Apple devices.
void apple_ble_flood();

/// Generic BLE advertisement flood with random MACs and payloads.
/// Saturates all three BLE advertising channels at the fastest legal interval.
void ble_adv_flood();

/// BLE scanner + advertiser flood.
/// Continuous active-scan + simultaneous advertising to disrupt connection
/// establishment handshakes between BLE peers.
void ble_scan_flood();

#endif // BLE_ATTACK_H
