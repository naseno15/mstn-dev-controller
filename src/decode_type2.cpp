#include "decode_type2.h"
#include <cmath>
#include <cstdlib>

NotchCalib default_type2_calib() {
  // Anchored on local dump MS=0x81 ≈ P0; BR spacing from common Type2 RE tables.
  // Override via profile JSON when hardware differs.
  NotchCalib c;
  c.power_raw = {0x81, 0x6D, 0x59, 0x45, 0x31, 0x1D}; // P0..P5
  c.brake_raw = {0xDF, 0xC4, 0xA9, 0x8E, 0x73, 0x58, 0x3D, 0x28, 0x16, 0x09}; // N,B1..B8,EB
  return c;
}

int nearest_notch(int raw, const std::vector<int>& table) {
  if (table.empty()) return 0;
  int best = 0;
  int best_d = std::abs(raw - table[0]);
  for (size_t i = 1; i < table.size(); ++i) {
    const int d = std::abs(raw - table[i]);
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}

void decode_type2_packet(std::span<const uint8_t> pkt, const NotchCalib& cal, ControllerState& out) {
  out.raw.fill(0);
  out.raw_len = static_cast<int>(pkt.size() < out.raw.size() ? pkt.size() : out.raw.size());
  for (int i = 0; i < out.raw_len; ++i) out.raw[static_cast<size_t>(i)] = pkt[static_cast<size_t>(i)];

  if (pkt.size() < 6) return;

  const int br = pkt[1];
  const int ms = pkt[2];
  const uint8_t pd = pkt[3];
  const uint8_t ht = pkt[4];
  const uint8_t bt = pkt[5];

  out.power_max = static_cast<int>(cal.power_raw.empty() ? 5 : cal.power_raw.size() - 1);
  out.brake_max = static_cast<int>(cal.brake_raw.empty() ? 9 : cal.brake_raw.size() - 1);
  out.power = nearest_notch(ms, cal.power_raw);
  out.brake = nearest_notch(br, cal.brake_raw);
  out.pedal = (pd == 0x00);

  // Provisional bit map — live UI shows raw HT/BT so remap is easy.
  // HT nibble: common Type2 d-pad packing
  out.hat_u = (ht & 0x01) != 0;
  out.hat_r = (ht & 0x02) != 0;
  out.hat_d = (ht & 0x04) != 0;
  out.hat_l = (ht & 0x08) != 0;
  out.btn_select = (ht & 0x10) != 0;
  out.btn_start = (ht & 0x20) != 0;

  out.btn_a = (bt & 0x01) != 0;
  out.btn_b = (bt & 0x02) != 0;
  out.btn_c = (bt & 0x04) != 0;
  out.btn_d = (bt & 0x08) != 0;

  out.bits.fill(false);
  for (int i = 0; i < 8; ++i) out.bits[static_cast<size_t>(i)] = ((ht >> i) & 1) != 0;
  for (int i = 0; i < 8; ++i) out.bits[static_cast<size_t>(8 + i)] = ((bt >> i) & 1) != 0;
}
