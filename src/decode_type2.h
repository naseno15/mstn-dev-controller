#pragma once
#include "controller_state.h"
#include <cstdint>
#include <span>
#include <vector>

struct NotchCalib {
  std::vector<int> power_raw;  // index = notch 0..N
  std::vector<int> brake_raw;  // index = notch 0..N (last = EB)
};

// Defaults from community Type2 dumps + local probe values (editable via profile).
NotchCalib default_type2_calib();

// Returns notch index, or -1 when raw is transition (0xFF).
int nearest_notch(int raw, const std::vector<int>& table);

// Packet: [0]=01 [1]=BR [2]=MS [3]=PD [4]=HT [5]=BT
void decode_type2_packet(std::span<const uint8_t> pkt, const NotchCalib& cal, ControllerState& out);
