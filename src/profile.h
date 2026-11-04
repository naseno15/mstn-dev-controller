#pragma once
#include "decode_type2.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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
  PushMode push = PushMode::Click;
  bool output_enabled = true;
  bool disable_feedback = false;
  KeyMap keys;
  NotchCalib calib = default_type2_calib();
};

bool load_profile(const std::wstring& path, Profile& out, std::string& err);
bool save_profile(const std::wstring& path, const Profile& in, std::string& err);
Profile default_bve6_profile();

WORD vk_from_name(const std::string& name);
std::wstring device_vidpid_needle(const std::string& kind);
