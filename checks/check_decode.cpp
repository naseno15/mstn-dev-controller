#undef NDEBUG
#include "decode_type2.h"

#include <cassert>
#include <cstdio>
#include <span>

int main() {
  const auto cal = default_type2_calib();
  assert(nearest_notch(0x81, cal.power_raw) == 0);
  assert(nearest_notch(0x1D, cal.power_raw) == 5);
  assert(nearest_notch(0xDF, cal.brake_raw) == 0);
  assert(nearest_notch(0x09, cal.brake_raw) == static_cast<int>(cal.brake_raw.size() - 1));

  // Local dump snapshot: BR=79 MS=81 PD=FF → near mid brake, P0, pedal up
  const uint8_t pkt[] = {0x01, 0x79, 0x81, 0xFF, 0x08, 0x00};
  ControllerState st;
  decode_type2_packet(std::span<const uint8_t>(pkt, 6), cal, st);
  assert(st.power == 0);
  assert(st.pedal == false);
  assert(st.brake >= 0 && st.brake <= st.brake_max);

  const uint8_t ped[] = {0x01, 0x79, 0x81, 0x00, 0x08, 0x00};
  decode_type2_packet(std::span<const uint8_t>(ped, 6), cal, st);
  assert(st.pedal == true);

  std::printf("check_decode: ok\n");
  return 0;
}
