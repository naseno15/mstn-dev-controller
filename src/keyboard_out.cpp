#include "keyboard_out.h"

#ifdef __APPLE__
#import <AppKit/AppKit.h>

#include <chrono>
#endif

namespace {

#ifdef __APPLE__
ULONGLONG GetTickCount64() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Needs Accessibility permission; without it macOS drops the event silently.
void send_vk(WORD vk, bool down) {
  const int code = mac_keycode(vk);
  if (code < 0) return;
  CGEventRef ev = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(code), down);
  if (!ev) return;
  CGEventPost(kCGHIDEventTap, ev);
  CFRelease(ev);
}
#else
bool is_extended_vk(WORD vk) {
  return vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT ||
         vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END ||
         vk == VK_PRIOR || vk == VK_NEXT || vk == VK_RCONTROL || vk == VK_RMENU ||
         vk == VK_NUMLOCK || vk == VK_DIVIDE || vk == VK_RSHIFT;
}

void send_vk(WORD vk, bool down) {
  if (!vk) return;
  INPUT in{};
  in.type = INPUT_KEYBOARD;
  in.ki.wVk = vk;
  in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
  in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  if (is_extended_vk(vk)) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  SendInput(1, &in, sizeof(INPUT));
}
#endif

} // namespace

bool KeyboardOut::should_block_inject() const {
#ifdef __APPLE__
  return NSApp.isActive; // never type into our own window
#else
  // Never inject into our own process (main window OR WebView2 child focus).
  HWND fg = GetForegroundWindow();
  if (!fg) return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(fg, &pid);
  if (pid == GetCurrentProcessId()) return true;
  if (ignore_hwnd_ && fg == ignore_hwnd_) return true;
  return false;
#endif
}

void KeyboardOut::flush_pending_ups() {
  if (pending_ups_.empty()) return;
  const ULONGLONG now = GetTickCount64();
  size_t w = 0;
  for (size_t i = 0; i < pending_ups_.size(); ++i) {
    if (now >= pending_ups_[i].due_ms) {
      send_vk(pending_ups_[i].vk, false);
    } else {
      pending_ups_[w++] = pending_ups_[i];
    }
  }
  pending_ups_.resize(w);
}

void KeyboardOut::tap(WORD vk, DWORD hold_ms) {
  if (!vk) return;
  send_vk(vk, true);
  const DWORD ms = hold_ms > 0 ? hold_ms : 50;
  pending_ups_.push_back(PendingUp{vk, GetTickCount64() + ms});
}

void KeyboardOut::set_down(WORD vk, bool down) {
  if (!vk) return;
  send_vk(vk, down);
}

void KeyboardOut::release_all() {
  for (const auto& p : pending_ups_) send_vk(p.vk, false);
  pending_ups_.clear();
  if (held_pedal_) {
    set_down(vk_from_name(profile_.keys.pedal), false);
    held_pedal_ = false;
  }
  for (WORD& vk : held_btn_vk_) {
    if (vk) {
      set_down(vk, false);
      vk = 0;
    }
  }
  last_pedal_ = last_a_ = last_b_ = last_c_ = last_d_ = false;
  last_sel_ = last_start_ = false;
  last_hu_ = last_hd_ = last_hl_ = last_hr_ = false;
}

bool KeyboardOut::step_gate() {
  const ULONGLONG now = GetTickCount64();
  const ULONGLONG gap = static_cast<ULONGLONG>(
      profile_.hold_interval_ms > 0 ? profile_.hold_interval_ms : 1.0);
  if (now - last_step_ms_ < gap) return false;
  last_step_ms_ = now;
  return true;
}

void KeyboardOut::step_axis(int& cur, int target, WORD up_vk, WORD down_vk) {
  if (cur == target) return;
  if (!step_gate()) return;
  if (target > cur) {
    tap(up_vk, 40);
    ++cur;
  } else {
    tap(down_vk, 40);
    --cur;
  }
}

// Stretch controller 0..ctrl_max onto the vehicle's real 0..veh_max.
// Ends stay pinned (0 and full). Unknown vehicle (veh_max<=0) stays 1:1.
int map_notch(int raw, int ctrl_max, int veh_max) {
  if (raw < 0) raw = 0;
  if (ctrl_max <= 0) return raw;
  if (veh_max <= 0 || veh_max == ctrl_max) return raw > ctrl_max ? ctrl_max : raw;
  if (raw <= 0) return 0;
  if (raw >= ctrl_max) return veh_max;
  return (raw * veh_max + ctrl_max / 2) / ctrl_max;
}

void KeyboardOut::tick(const ControllerState& st) {
  flush_pending_ups();
  if (!profile_.output_enabled || !st.connected) return;
  if (should_block_inject()) return;

  const int extra = profile_.notch_overshoot < 0 ? 0 : profile_.notch_overshoot;

  const WORD p_up = vk_from_name(profile_.keys.power_up);
  const WORD p_dn = vk_from_name(profile_.keys.power_down);
  const WORD p_off = vk_from_name(profile_.keys.power_off);
  const int ctrl_pmax = st.power_max > 0 ? st.power_max : profile_.power_steps;
  const int pt = map_notch(st.power, ctrl_pmax, st.veh_power_max);
  const int pmax = st.veh_power_max > 0 ? st.veh_power_max : ctrl_pmax;

  if (pt != power_cmd_) {
    power_cmd_ = pt;
    power_overshoot_ = (extra > 0 && (pt == 0 || pt == pmax)) ? extra : 0;
  }

  if (pt == 0) {
    if (cur_power_ != 0) {
      if (!p_off) {
        step_axis(cur_power_, 0, p_up, p_dn); // one down-tap only drops one notch
      } else if (step_gate()) {
        tap(p_off, 40);
        cur_power_ = 0;
      }
    } else if (power_overshoot_ > 0 && step_gate()) {
      tap(p_off ? p_off : p_dn, 40);
      --power_overshoot_;
    }
  } else if (cur_power_ != pt) {
    step_axis(cur_power_, pt, p_up, p_dn);
  } else if (pt == pmax && power_overshoot_ > 0 && step_gate()) {
    tap(p_up, 40);
    --power_overshoot_;
  }

  const WORD b_up = vk_from_name(profile_.keys.brake_up);
  const WORD b_dn = vk_from_name(profile_.keys.brake_down);
  const WORD b_eb = vk_from_name(profile_.keys.brake_eb);
  const int ctrl_bmax = st.brake_max > 0 ? st.brake_max : (profile_.brake_steps + 1);
  const int bt = map_notch(st.brake, ctrl_bmax, st.veh_brake_max);
  const int eb_idx = st.veh_brake_max > 0 ? st.veh_brake_max : ctrl_bmax;

  if (bt != brake_cmd_) {
    brake_cmd_ = bt;
    brake_overshoot_ = (extra > 0 && (bt == 0 || bt == eb_idx)) ? extra : 0;
  }

  if (bt == eb_idx && eb_idx > 0) {
    if (cur_brake_ != eb_idx) {
      if (step_gate()) {
        tap(b_eb ? b_eb : b_up, 40);
        cur_brake_ = eb_idx;
      }
    } else if (brake_overshoot_ > 0 && step_gate()) {
      tap(b_eb ? b_eb : b_up, 40);
      --brake_overshoot_;
    }
  } else if (cur_brake_ != bt) {
    if (cur_brake_ == eb_idx && eb_idx > 0) {
      if (step_gate()) {
        tap(b_dn, 40);
        cur_brake_ = eb_idx - 1;
      }
    } else {
      step_axis(cur_brake_, bt, b_up, b_dn);
    }
  } else if (bt == 0 && brake_overshoot_ > 0 && step_gate()) {
    tap(b_dn, 40);
    --brake_overshoot_;
  }

  constexpr DWORD kBtnTapMs = 50;

  auto edge_btn = [&](bool now, bool& last, const std::string& key, WORD& held_slot) {
    const WORD vk = vk_from_name(key);
    if (!vk) {
      if (held_slot) {
        set_down(held_slot, false);
        held_slot = 0;
      }
      last = now;
      return;
    }
    if (profile_.push == PushMode::Click) {
      if (held_slot) {
        set_down(held_slot, false);
        held_slot = 0;
      }
      if (now && !last) tap(vk, kBtnTapMs);
    } else {
      if (now != last) {
        set_down(vk, now);
        held_slot = now ? vk : 0;
      }
    }
    last = now;
  };

  edge_btn(st.btn_a, last_a_, profile_.keys.btn_a, held_btn_vk_[0]);
  edge_btn(st.btn_b, last_b_, profile_.keys.btn_b, held_btn_vk_[1]);
  edge_btn(st.btn_c, last_c_, profile_.keys.btn_c, held_btn_vk_[2]);
  edge_btn(st.btn_d, last_d_, profile_.keys.btn_d, held_btn_vk_[3]);
  edge_btn(st.btn_select, last_sel_, profile_.keys.btn_select, held_btn_vk_[4]);
  edge_btn(st.btn_start, last_start_, profile_.keys.btn_start, held_btn_vk_[5]);
  edge_btn(st.hat_u, last_hu_, profile_.keys.hat_u, held_btn_vk_[6]);
  edge_btn(st.hat_d, last_hd_, profile_.keys.hat_d, held_btn_vk_[7]);
  edge_btn(st.hat_l, last_hl_, profile_.keys.hat_l, held_btn_vk_[8]);
  edge_btn(st.hat_r, last_hr_, profile_.keys.hat_r, held_btn_vk_[9]);

  const WORD ped = vk_from_name(profile_.keys.pedal);
  if (st.pedal != last_pedal_) {
    if (profile_.push == PushMode::Click && st.pedal) {
      tap(ped, kBtnTapMs);
    } else {
      set_down(ped, st.pedal);
      held_pedal_ = st.pedal;
    }
    last_pedal_ = st.pedal;
  }
}
