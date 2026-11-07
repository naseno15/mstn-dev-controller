#pragma once
#include "decode_type2.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdint>
// Windows VK codes stay the portable key id (profiles, 0 = unbound); macOS maps them at send time.
using WORD = uint16_t;
using DWORD = uint32_t;
using ULONGLONG = uint64_t;
enum : WORD {
  VK_BACK = 0x08, VK_TAB = 0x09, VK_RETURN = 0x0D, VK_ESCAPE = 0x1B, VK_SPACE = 0x20,
  VK_PRIOR = 0x21, VK_NEXT = 0x22, VK_END = 0x23, VK_HOME = 0x24,
  VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28,
  VK_INSERT = 0x2D, VK_DELETE = 0x2E, VK_ADD = 0x6B,
  VK_OEM_1 = 0xBA, VK_OEM_PLUS = 0xBB, VK_OEM_COMMA = 0xBC, VK_OEM_MINUS = 0xBD,
  VK_OEM_PERIOD = 0xBE, VK_OEM_2 = 0xBF, VK_OEM_4 = 0xDB, VK_OEM_5 = 0xDC,
  VK_OEM_6 = 0xDD, VK_OEM_7 = 0xDE,
};
#endif

#include <filesystem>
#include <string>

enum class PushMode { Press, Click };

struct KeyMap {
  std::string power_up = "Z";
  std::string power_down = "A";
  std::string power_off = "Q";
  std::string brake_up = "OEM_PERIOD";
  std::string brake_down = "OEM_COMMA";
  std::string brake_eb = "OEM_2";
  std::string pedal = "SPACE";
  std::string btn_a = "SPACE";
  std::string btn_b = "B";
  std::string btn_c = "C";
  std::string btn_d = "D";
  std::string btn_select = "RETURN";
  std::string btn_start = "";
  std::string hat_u = "UP";
  std::string hat_d = "DOWN";
  std::string hat_l = "LEFT";
  std::string hat_r = "RIGHT";
};

struct Profile {
  std::string name = "bve6-type2";
  std::string device_kind = "type2";
  std::string game_preset = "bve6";
  int power_steps = 5;
  int brake_steps = 8;
  double hold_interval_ms = 1.0;
  // Extra key taps after reaching 0 / max / EB. 0 = off (default).
  int notch_overshoot = 0;
  PushMode push = PushMode::Click;
  bool output_enabled = true;
  bool disable_feedback = false;
  KeyMap keys;
  NotchCalib calib = default_type2_calib();
};

bool load_profile(const std::filesystem::path& path, Profile& out, std::string& err);
bool save_profile(const std::filesystem::path& path, const Profile& in, std::string& err);
Profile default_bve6_profile();

WORD vk_from_name(const std::string& name);
std::wstring device_vidpid_needle(const std::string& kind);

#ifdef __APPLE__
// macOS virtual keycode (kVK_*, ANSI layout) for a Windows VK, or -1 when unmapped.
int mac_keycode(WORD vk);
#endif
