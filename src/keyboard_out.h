#pragma once
#include "controller_state.h"
#include "profile.h"
#include <windows.h>

class KeyboardOut {
public:
  void set_profile(const Profile& p) { profile_ = p; }
  void set_ignore_hwnd(HWND hwnd) { ignore_hwnd_ = hwnd; }

  // Call from main/UI thread with latest decoded state.
  void tick(const ControllerState& st);

  void release_all();

private:
  void tap(WORD vk);
  void set_down(WORD vk, bool down);
  void step_axis(int& cur, int target, WORD up_vk, WORD down_vk, WORD extra_vk, bool extra_at_max);

  Profile profile_{};
  HWND ignore_hwnd_ = nullptr;
  int cur_power_ = 0;
  int cur_brake_ = 0;
  bool last_pedal_ = false;
  bool last_a_ = false, last_b_ = false, last_c_ = false, last_d_ = false;
  bool last_sel_ = false, last_start_ = false;
  bool last_hu_ = false, last_hd_ = false, last_hl_ = false, last_hr_ = false;
  bool held_pedal_ = false;
  ULONGLONG last_step_ms_ = 0;
};
