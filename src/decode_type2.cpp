#include "decode_type2.h"
#include <cstdlib>

NotchCalib default_type2_calib() {
  // Official Type2 USB values (traincontrollerdb / TCPP-20009):
  // Brake Released=0x79 … EB=0xB9 (increases with brake)
  // Power N=0x81 … P5=0x00 (decreases with power)
  // 0xFF = transition — ignored by nearest_notch
  NotchCalib c;
  c.power_raw = {0x81, 0x6D, 0x54, 0x3F, 0x21, 0x00};                 // P0..P5
  c.brake_raw = {0x79, 0x8A, 0x94, 0x9A, 0xA2, 0xA8, 0xAF, 0xB2, 0xB5, 0xB9}; // N,B1..B8,EB
  return c;
}

int nearest_notch(int raw, const std::vector<int>& table) {
  if (table.empty()) return 0;
  if (raw == 0xFF) return -1; // transition between detents
  // Clamp past end-stop: Type2 EB is always 0xB9; overtravel still reports EB
  if (raw >= table.back() && table.back() >= table.front())
    return static_cast<int>(table.size() - 1);
  // Power table decreases (N=0x81 … P5=0x00): values at/below last = max power
  if (table.back() < table.front() && raw <= table.back())
    return static_cast<int>(table.size() - 1);
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

  const int p = nearest_notch(ms, cal.power_raw);
  const int b = nearest_notch(br, cal.brake_raw);
  if (p >= 0) out.power = p;
  if (b >= 0) out.brake = b;

  out.pedal = (pd == 0x00);

  // D-pad is POV hat value, not a bitmask (None=0x08)
  out.hat_u = out.hat_d = out.hat_l = out.hat_r = false;
  switch (ht) {
  case 0x00: out.hat_u = true; break;
  case 0x01: out.hat_u = out.hat_r = true; break;
  case 0x02: out.hat_r = true; break;
  case 0x03: out.hat_d = out.hat_r = true; break;
  case 0x04: out.hat_d = true; break;
  case 0x05: out.hat_d = out.hat_l = true; break;
  case 0x06: out.hat_l = true; break;
  case 0x07: out.hat_u = out.hat_l = true; break;
  default: break; // 0x08 = none
  }

  // Buttons byte: bit0=B, bit1=A, bit2=C, bit3=D, bit4=Select, bit5=Start
  out.btn_b = (bt & 0x01) != 0;
  out.btn_a = (bt & 0x02) != 0;
  out.btn_c = (bt & 0x04) != 0;
  out.btn_d = (bt & 0x08) != 0;
  out.btn_select = (bt & 0x10) != 0;
  out.btn_start = (bt & 0x20) != 0;

  out.bits.fill(false);
  // Show hat as lamps 0-7 coded, buttons 8-13
  if (ht <= 0x07) out.bits[ht] = true;
  for (int i = 0; i < 6; ++i) out.bits[static_cast<size_t>(8 + i)] = ((bt >> i) & 1) != 0;
}
