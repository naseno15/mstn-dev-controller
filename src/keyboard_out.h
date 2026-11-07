#pragma once
#include "controller_state.h"
#include "profile.h"

#include <vector>

class KeyboardOut {
public:
  void set_profile(const Profile& p) { profile_ = p; }
#ifdef _WIN32
  void set_ignore_hwnd(HWND hwnd) { ignore_hwnd_ = hwnd; }
#endif

  // Call from main/UI thread with latest decoded state.
  void tick(const ControllerState& st);

  void release_all();

private:
  void tap(WORD vk, DWORD hold_ms = 50);
  void set_down(WORD vk, bool down);
  void flush_pending_ups();
  void step_axis(int& cur, int target, WORD up_vk, WORD down_vk);
  bool step_gate(); // rate-limit notch taps; true = may send one now
  bool should_block_inject() const;

  struct PendingUp {
    WORD vk = 0;
    ULONGLONG due_ms = 0;
  };

  Profile profile_{};
#ifdef _WIN32
  HWND ignore_hwnd_ = nullptr;
#endif
  int cur_power_ = 0;
  int cur_brake_ = 0;
  int power_cmd_ = -1;   // last commanded target (re-arms overshoot on change)
  int brake_cmd_ = -1;
  int power_overshoot_ = 0;
  int brake_overshoot_ = 0;
  bool last_pedal_ = false;
  bool last_a_ = false, last_b_ = false, last_c_ = false, last_d_ = false;
  bool last_sel_ = false, last_start_ = false;
  bool last_hu_ = false, last_hd_ = false, last_hl_ = false, last_hr_ = false;
  bool held_pedal_ = false;
  WORD held_btn_vk_[12]{}; // PRESS mode sticky downs (A..hat_r)
  ULONGLONG last_step_ms_ = 0;
  std::vector<PendingUp> pending_ups_;
};
