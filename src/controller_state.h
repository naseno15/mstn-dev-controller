#pragma once
#include <array>
#include <cstdint>
#include <string>

struct ControllerState {
  bool connected = false;
  std::array<uint8_t, 8> raw{};
  int raw_len = 0;
  double hz = 0.0;

  int power = 0;
  int brake = 0;
  int power_max = 5;  // controller span (Type2 P0..P5)
  int brake_max = 9;  // controller span (N .. EB)
  // From BVE NotchInfo via denconvMMF. 0 = unknown → 1:1 with controller.
  int veh_power_max = 0;
  int veh_brake_max = 0;

  bool pedal = false;
  bool btn_a = false;
  bool btn_b = false;
  bool btn_c = false;
  bool btn_d = false;
  bool btn_select = false;
  bool btn_start = false;
  bool hat_u = false;
  bool hat_d = false;
  bool hat_l = false;
  bool hat_r = false;

  // 16 bit lamps (from HT/BT raw)
  std::array<bool, 16> bits{};

  bool door_lamp = false;
  bool vib_l = false;
  bool vib_r = false;
  int atc = 0;
  int speed = 0;

  std::string status = "idle";
  std::string driver = "unknown"; // ok | needs_winusb | missing | busy
};
