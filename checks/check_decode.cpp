#undef NDEBUG
#include "decode_type2.h"

#include <cassert>
#include <cstdio>
#include <span>

int main() {
  const auto cal = default_type2_calib();
  assert(nearest_notch(0x81, cal.power_raw) == 0);  // P0
  assert(nearest_notch(0x00, cal.power_raw) == 5);  // P5
  assert(nearest_notch(0x79, cal.brake_raw) == 0);  // Released
  assert(nearest_notch(0xB9, cal.brake_raw) == 9);  // EB
  assert(nearest_notch(0xC0, cal.brake_raw) == 9);  // overtravel clamps to EB
  assert(nearest_notch(0xFF, cal.power_raw) == -1); // transition

  // Local dump: BR=79 MS=81 PD=FF → N + P0, pedal up
  const uint8_t pkt[] = {0x01, 0x79, 0x81, 0xFF, 0x08, 0x00};
  ControllerState st;
  decode_type2_packet(std::span<const uint8_t>(pkt, 6), cal, st);
  assert(st.power == 0);
  assert(st.brake == 0);
  assert(st.pedal == false);

  // Pedal on + A button (bit1)
  const uint8_t ped[] = {0x01, 0x79, 0x81, 0x00, 0x08, 0x02};
  decode_type2_packet(std::span<const uint8_t>(ped, 6), cal, st);
  assert(st.pedal == true);
  assert(st.btn_a == true);
  assert(st.btn_b == false);

  // D-pad Up
  const uint8_t up[] = {0x01, 0x79, 0x81, 0xFF, 0x00, 0x00};
  decode_type2_packet(std::span<const uint8_t>(up, 6), cal, st);
  assert(st.hat_u == true);
  assert(st.hat_d == false);

  // EB + P5
  const uint8_t max[] = {0x01, 0xB9, 0x00, 0xFF, 0x08, 0x00};
  decode_type2_packet(std::span<const uint8_t>(max, 6), cal, st);
  assert(st.brake == 9);
  assert(st.power == 5);

  std::printf("check_decode: ok\n");
  return 0;
}
