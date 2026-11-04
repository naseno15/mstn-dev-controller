#include "keyboard_out.h"

namespace {

void send_vk(WORD vk, bool down) {
  if (!vk) return;
  INPUT in{};
  in.type = INPUT_KEYBOARD;
  in.ki.wVk = vk;
  in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
  in.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
  // Extended keys (arrows etc.)
  if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT ||
      vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END ||
      vk == VK_PRIOR || vk == VK_NEXT || vk == VK_RCONTROL || vk == VK_RMENU || vk == VK_NUMLOCK) {
    in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  SendInput(1, &in, sizeof(INPUT));
}

} // namespace

void KeyboardOut::tap(WORD vk) {
  if (!vk) return;
  send_vk(vk, true);
  Sleep(static_cast<DWORD>(profile_.hold_interval_ms > 0 ? profile_.hold_interval_ms : 1));
  send_vk(vk, false);
}

void KeyboardOut::set_down(WORD vk, bool down) {
  if (!vk) return;
  send_vk(vk, down);
}

void KeyboardOut::release_all() {
  if (held_pedal_) {
    set_down(vk_from_name(profile_.keys.pedal), false);
    held_pedal_ = false;
  }
  last_pedal_ = last_a_ = last_b_ = last_c_ = last_d_ = false;
  last_sel_ = last_start_ = false;
  last_hu_ = last_hd_ = last_hl_ = last_hr_ = false;
}

void KeyboardOut::step_axis(int& cur, int target, WORD up_vk, WORD down_vk, WORD extra_vk, bool extra_at_max) {
  if (cur == target) return;
  const ULONGLONG now = GetTickCount64();
  if (now - last_step_ms_ < static_cast<ULONGLONG>(profile_.hold_interval_ms)) return;
  last_step_ms_ = now;

  if (extra_at_max && target > cur && cur == profile_.brake_steps && extra_vk) {
    // approaching EB from B8
    tap(extra_vk);
    cur = target;
    return;
  }
  if (target > cur) {
    tap(up_vk);
    ++cur;
  } else {
    tap(down_vk);
    --cur;
  }
}

void KeyboardOut::tick(const ControllerState& st) {
  if (!profile_.output_enabled || !st.connected) return;
  if (ignore_hwnd_ && GetForegroundWindow() == ignore_hwnd_) return;

  // Power 0..power_max — BVE uses step up/down + off
  const WORD p_up = vk_from_name(profile_.keys.power_up);
  const WORD p_dn = vk_from_name(profile_.keys.power_down);
  const WORD p_off = vk_from_name(profile_.keys.power_off);
  if (st.power == 0 && cur_power_ != 0) {
    const ULONGLONG now = GetTickCount64();
    if (now - last_step_ms_ >= static_cast<ULONGLONG>(profile_.hold_interval_ms)) {
      tap(p_off);
      cur_power_ = 0;
      last_step_ms_ = now;
    }
  } else if (st.power != cur_power_) {
    step_axis(cur_power_, st.power, p_up, p_dn, 0, false);
  }

  // Brake 0=N .. brake_max-1=Bmax, brake_max=EB (our decode uses 9=EB with max 9)
  const WORD b_up = vk_from_name(profile_.keys.brake_up);
  const WORD b_dn = vk_from_name(profile_.keys.brake_down);
  const WORD b_eb = vk_from_name(profile_.keys.brake_eb);
  const int eb_idx = st.brake_max; // last index = EB
  if (st.brake == eb_idx) {
    if (cur_brake_ != eb_idx) {
      const ULONGLONG now = GetTickCount64();
      if (now - last_step_ms_ >= static_cast<ULONGLONG>(profile_.hold_interval_ms)) {
        tap(b_eb);
        cur_brake_ = eb_idx;
        last_step_ms_ = now;
      }
    }
  } else if (st.brake != cur_brake_) {
    if (cur_brake_ == eb_idx) {
      // leave EB via brake_down toward B8
      step_axis(cur_brake_, st.brake, b_up, b_dn, 0, false);
      if (cur_brake_ == eb_idx) cur_brake_ = eb_idx - 1;
    } else {
      step_axis(cur_brake_, st.brake, b_up, b_dn, 0, false);
    }
  }

  auto edge_btn = [&](bool now, bool& last, const std::string& key) {
    const WORD vk = vk_from_name(key);
    if (!vk) {
      last = now;
      return;
    }
    if (profile_.push == PushMode::Click) {
      if (now && !last) tap(vk);
    } else {
      if (now != last) set_down(vk, now);
    }
    last = now;
  };

  edge_btn(st.btn_a, last_a_, profile_.keys.btn_a);
  edge_btn(st.btn_b, last_b_, profile_.keys.btn_b);
  edge_btn(st.btn_c, last_c_, profile_.keys.btn_c);
  edge_btn(st.btn_d, last_d_, profile_.keys.btn_d);
  edge_btn(st.btn_select, last_sel_, profile_.keys.btn_select);
  edge_btn(st.btn_start, last_start_, profile_.keys.btn_start);
  edge_btn(st.hat_u, last_hu_, profile_.keys.hat_u);
  edge_btn(st.hat_d, last_hd_, profile_.keys.hat_d);
  edge_btn(st.hat_l, last_hl_, profile_.keys.hat_l);
  edge_btn(st.hat_r, last_hr_, profile_.keys.hat_r);

  // Pedal: hold while down (horn)
  const WORD ped = vk_from_name(profile_.keys.pedal);
  if (st.pedal != last_pedal_) {
    if (profile_.push == PushMode::Click && st.pedal) {
      tap(ped);
    } else {
      set_down(ped, st.pedal);
      held_pedal_ = st.pedal;
    }
    last_pedal_ = st.pedal;
  }
}
