#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <cstdio>

// Mirrors UsbType2 hariko layout: [function, on/off]
static void pack(uint8_t function, bool on, uint8_t out[2]) {
  out[0] = function;                       // 01 L / 02 R / 03 door
  out[1] = on ? 0x01 : 0x00;               // status
}

int main() {
  uint8_t b[2];
  pack(0x03, true, b);
  assert(b[0] == 0x03 && b[1] == 0x01);
  pack(0x01, true, b);
  assert(b[0] == 0x01 && b[1] == 0x01);
  pack(0x02, false, b);
  assert(b[0] == 0x02 && b[1] == 0x00);
  std::printf("check_feedback_pkt: ok\n");
  return 0;
}
