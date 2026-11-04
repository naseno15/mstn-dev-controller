#include "app_state.h"

#include <span>
#include <sstream>
#include <shellapi.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

namespace {
std::string hex_pkt(const ControllerState& st) {
  std::ostringstream o;
  o << std::uppercase << std::hex;
  for (int i = 0; i < st.raw_len; ++i) {
    if (i) o << '-';
    o.width(2);
    o.fill('0');
    o << static_cast<int>(st.raw[static_cast<size_t>(i)]);
  }
  return o.str();
}

std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  std::wstring w(n > 0 ? n - 1 : 0, L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
  return w;
}
} // namespace

AppState::AppState() {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  PathRemoveFileSpecW(path);
  exe_dir_ = path;
  profile_ = default_bve6_profile();
  calib_ = profile_.calib;
  std::string err;
  load_profile_path(default_profile_path(), err);
  mmf_.open();
  usb_.set_needle(device_vidpid_needle(profile_.device_kind));
  usb_.set_on_packet([this](const uint8_t* d, int n, double hz) { on_packet(d, n, hz); });
  usb_.set_on_status([this](const std::string& s) { on_usb_status(s); });
}

AppState::~AppState() {
  usb_.stop();
  keys_.release_all();
  mmf_.close();
}

void AppState::set_ui_push(UiPushFn fn) { ui_push_ = std::move(fn); }

void AppState::set_hwnd(HWND hwnd) {
  hwnd_ = hwnd;
  keys_.set_ignore_hwnd(hwnd);
}

std::wstring AppState::exe_dir() const { return exe_dir_; }

std::wstring AppState::default_profile_path() const {
  return exe_dir_ + L"\\profiles\\bve6-type2.json";
}

std::wstring AppState::preset_path_for(const std::string& game, const std::string& device) const {
  return exe_dir_ + L"\\profiles\\" + utf8_to_wide(game + "-" + device + ".json");
}

void AppState::start_usb() {
  usb_.set_needle(device_vidpid_needle(profile_.device_kind));
  usb_.start();
  push_toast("toast_started", profile_.device_kind);
}

void AppState::stop_usb() {
  usb_.stop();
  push_toast("toast_stopped", "");
}

bool AppState::load_profile_path(const std::wstring& path, std::string& err) {
  Profile p;
  if (!::load_profile(path, p, err)) return false;
  {
    std::lock_guard lock(mu_);
    profile_ = p;
    calib_ = p.calib;
    keys_.set_profile(profile_);
  }
  usb_.set_needle(device_vidpid_needle(p.device_kind));
  push_toast("toast_loaded", p.name);
  return true;
}

bool AppState::save_profile_path(const std::wstring& path, std::string& err) {
  bool ok = false;
  {
    std::lock_guard lock(mu_);
    ok = ::save_profile(path, profile_, err);
  }
  if (ok) push_toast("toast_saved", "");
  return ok;
}

bool AppState::save_game_preset(const std::string& game, const std::string& device, std::string& err) {
  {
    std::lock_guard lock(mu_);
    if (!game.empty()) profile_.game_preset = game;
    if (!device.empty()) profile_.device_kind = device;
    profile_.name = profile_.game_preset + "-" + profile_.device_kind;
  }
  CreateDirectoryW((exe_dir_ + L"\\profiles").c_str(), nullptr);
  const auto path = preset_path_for(profile_.game_preset, profile_.device_kind);
  return save_profile_path(path, err);
}

void AppState::apply_output_settings(bool enabled, const std::string& push, double hold_ms,
                                     bool disable_fb, const std::string& device_kind,
                                     const std::string& game_preset) {
  std::string from_dev, to_dev;
  {
    std::lock_guard lock(mu_);
    from_dev = profile_.device_kind;
    profile_.output_enabled = enabled;
    profile_.push = (push == "PRESS" || push == "press") ? PushMode::Press : PushMode::Click;
    if (hold_ms >= 0) profile_.hold_interval_ms = hold_ms;
    profile_.disable_feedback = disable_fb;
    if (!device_kind.empty()) profile_.device_kind = device_kind;
    if (!game_preset.empty()) profile_.game_preset = game_preset;
    to_dev = profile_.device_kind;
    keys_.set_profile(profile_);
  }
  usb_.set_needle(device_vidpid_needle(to_dev));
  if (disable_fb) mmf_.clear_feedback();
  push_toast("toast_applied", from_dev + " → " + to_dev);
}

void AppState::apply_keymap_field(const std::string& field, const std::string& value) {
  std::lock_guard lock(mu_);
  auto& k = profile_.keys;
  if (field == "power_up") k.power_up = value;
  else if (field == "power_down") k.power_down = value;
  else if (field == "power_off") k.power_off = value;
  else if (field == "brake_up") k.brake_up = value;
  else if (field == "brake_down") k.brake_down = value;
  else if (field == "brake_eb") k.brake_eb = value;
  else if (field == "pedal") k.pedal = value;
  else if (field == "btn_a") k.btn_a = value;
  else if (field == "btn_b") k.btn_b = value;
  else if (field == "btn_c") k.btn_c = value;
  else if (field == "btn_d") k.btn_d = value;
  else if (field == "btn_select") k.btn_select = value;
  else if (field == "btn_start") k.btn_start = value;
  else if (field == "hat_u") k.hat_u = value;
  else if (field == "hat_d") k.hat_d = value;
  else if (field == "hat_l") k.hat_l = value;
  else if (field == "hat_r") k.hat_r = value;
  keys_.set_profile(profile_);
}

void AppState::push_toast(const std::string& title, const std::string& body) {
  std::lock_guard lock(mu_);
  toast_title_ = title;
  toast_body_ = body;
  toast_pending_ = true;
}

void AppState::test_door() {
  if (profile_.disable_feedback) {
    push_toast("toast_applied", "feedback disabled");
    return;
  }
  // Toggle door lamp; stay on until toggled off / test_off (no auto-clear).
  bool next = true;
  {
    std::lock_guard lock(mu_);
    next = !live_.door_lamp;
    live_.door_lamp = next;
    // keep vib as-is
  }
  bool vib_l = false, vib_r = false;
  {
    std::lock_guard lock(mu_);
    vib_l = live_.vib_l;
    vib_r = live_.vib_r;
  }
  mmf_.write_feedback(next, vib_l, vib_r);
  test_frames_left_ = 0; // do not auto-clear door
  const bool usb_ok = usb_.send_feedback(next, vib_l, vib_r);
  if (!usb_.running())
    push_toast("toast_test_door", "กด Start ก่อน");
  else if (!usb_ok)
    push_toast("toast_test_door", usb_.last_fb_error());
  else
    push_toast("toast_test_door", next ? "ไฟประตู ON" : "ไฟประตู OFF");
}

void AppState::test_vib() { test_vib_set(true, true); }

void AppState::test_vib_on() { test_vib_set(true, true); }

void AppState::test_vib_off() { test_vib_set(false, false); }

void AppState::test_vib_set(bool left, bool right) {
  if (profile_.disable_feedback) {
    push_toast("toast_applied", "feedback disabled");
    return;
  }
  bool door = false;
  {
    std::lock_guard lock(mu_);
    door = live_.door_lamp;
    live_.vib_l = left;
    live_.vib_r = right;
  }
  mmf_.write_feedback(door, left, right);
  test_frames_left_ = 0;
  const bool usb_ok = usb_.send_feedback(door, left, right);
  if (!left && !right) return; // quiet on release
  if (!usb_.running())
    push_toast("toast_test_vib", "กด Start ก่อน");
  else if (!usb_ok)
    push_toast("toast_test_vib", usb_.last_fb_error());
  else {
    const char* which = (left && right) ? "L+R" : left ? "L" : "R";
    push_toast("toast_test_vib", which);
  }
}

void AppState::test_off() {
  mmf_.clear_feedback();
  test_frames_left_ = 0;
  usb_.send_feedback(false, false, false);
  {
    std::lock_guard lock(mu_);
    live_.door_lamp = false;
    live_.vib_l = false;
    live_.vib_r = false;
  }
  push_toast("toast_test_off", "");
}

void AppState::probe_driver() {
  // Already holding live WinUSB → treat as OK (CreateFile would falsely look busy/bad)
  if (usb_.has_handle()) {
    {
      std::lock_guard lock(mu_);
      live_.driver = "ok";
      driver_prompt_ = false;
    }
    push_toast("toast_driver_ok", "in use by this app");
    return;
  }

  const auto st = UsbType2::probe_driver(device_vidpid_needle(profile_.device_kind));
  {
    std::lock_guard lock(mu_);
    live_.driver = st.rfind("wrong_driver", 0) == 0 ? "needs_winusb" : st;
    driver_prompt_ = (st == "needs_winusb" || st.rfind("wrong_driver", 0) == 0);
  }
  if (st == "ok") {
    push_toast("toast_driver_ok", "WinUSB");
  } else if (st == "busy") {
    push_toast("toast_driver_busy", "ปิด denconv / แอปอื่นก่อน");
  } else if (st == "missing") {
    push_toast("toast_driver_missing", "เสียบ Type2 แล้วลองใหม่");
  } else if (st.rfind("wrong_driver:", 0) == 0) {
    push_toast("toast_driver_wrong", st.substr(13) + " → เปลี่ยนเป็น WinUSB ใน Zadig");
  } else if (st == "needs_winusb") {
    push_toast("toast_driver_bad", "Zadig → WinUSB");
  } else {
    push_toast("toast_driver_bad", st);
  }
}

void AppState::open_zadig() {
  ShellExecuteW(hwnd_, L"open", L"https://zadig.akeo.ie/", nullptr, nullptr, SW_SHOWNORMAL);
  push_toast("toast_applied", "Zadig");
}

void AppState::on_packet(const uint8_t* data, int len, double hz) {
  ControllerState st;
  {
    std::lock_guard lock(mu_);
    st = live_;
  }
  st.connected = true;
  st.driver = "ok";
  st.hz = hz;
  decode_type2_packet(std::span<const uint8_t>(data, static_cast<size_t>(len)), calib_, st);
  st.status = "live";
  {
    std::lock_guard lock(mu_);
    live_ = st;
  }
}

void AppState::on_usb_status(const std::string& s) {
  std::lock_guard lock(mu_);
  // Friendly status codes for UI (not raw WinUSB jargon)
  if (s == "ready" || s.rfind("connected", 0) == 0) {
    live_.status = "ready";
    live_.connected = true;
    live_.driver = "ok";
  } else if (s == "searching device") {
    live_.status = "searching";
  } else if (s == "needs_winusb" || s.rfind("wrong_driver", 0) == 0) {
    live_.status = "needs_winusb";
    live_.driver = "needs_winusb";
    live_.connected = false;
    driver_prompt_ = true;
    toast_title_ = s.rfind("wrong_driver", 0) == 0 ? "toast_driver_wrong" : "toast_driver_bad";
    toast_body_ = s.rfind("wrong_driver:", 0) == 0 ? s.substr(13) : s;
    toast_pending_ = true;
  } else if (s.find("not found") != std::string::npos) {
    live_.status = "missing";
    live_.connected = false;
    live_.driver = "missing";
  } else if (s == "busy" || s.find("busy") != std::string::npos || s.find("failed") != std::string::npos) {
    live_.status = "busy";
    live_.connected = false;
  } else if (s.find("disconnected") != std::string::npos) {
    live_.status = "idle";
    live_.connected = false;
  } else {
    live_.status = s;
  }
}

std::string AppState::state_to_json(const ControllerState& st) const {
  std::string status = st.status;
  for (char& c : status) if (c == '"' || c == '\\') c = '\'';

  std::ostringstream o;
  o << "{\"type\":\"state\""
    << ",\"connected\":" << (st.connected ? "true" : "false")
    << ",\"status\":\"" << status << "\""
    << ",\"driver\":\"" << st.driver << "\""
    << ",\"hz\":" << st.hz
    << ",\"hex\":\"" << hex_pkt(st) << "\""
    << ",\"br\":" << (st.raw_len > 1 ? static_cast<int>(st.raw[1]) : -1)
    << ",\"ms\":" << (st.raw_len > 2 ? static_cast<int>(st.raw[2]) : -1)
    << ",\"pd\":" << (st.raw_len > 3 ? static_cast<int>(st.raw[3]) : -1)
    << ",\"ht\":" << (st.raw_len > 4 ? static_cast<int>(st.raw[4]) : -1)
    << ",\"bt\":" << (st.raw_len > 5 ? static_cast<int>(st.raw[5]) : -1)
    << ",\"power\":" << st.power
    << ",\"brake\":" << st.brake
    << ",\"power_max\":" << st.power_max
    << ",\"brake_max\":" << st.brake_max
    << ",\"pedal\":" << (st.pedal ? "true" : "false")
    << ",\"btn_a\":" << (st.btn_a ? "true" : "false")
    << ",\"btn_b\":" << (st.btn_b ? "true" : "false")
    << ",\"btn_c\":" << (st.btn_c ? "true" : "false")
    << ",\"btn_d\":" << (st.btn_d ? "true" : "false")
    << ",\"btn_select\":" << (st.btn_select ? "true" : "false")
    << ",\"btn_start\":" << (st.btn_start ? "true" : "false")
    << ",\"hat_u\":" << (st.hat_u ? "true" : "false")
    << ",\"hat_d\":" << (st.hat_d ? "true" : "false")
    << ",\"hat_l\":" << (st.hat_l ? "true" : "false")
    << ",\"hat_r\":" << (st.hat_r ? "true" : "false")
    << ",\"door\":" << (st.door_lamp ? "true" : "false")
    << ",\"vib_l\":" << (st.vib_l ? "true" : "false")
    << ",\"vib_r\":" << (st.vib_r ? "true" : "false")
    << ",\"atc\":" << st.atc
    << ",\"speed\":" << st.speed
    << ",\"output\":" << (profile_.output_enabled ? "true" : "false")
    << ",\"disable_feedback\":" << (profile_.disable_feedback ? "true" : "false")
    << ",\"push\":\"" << (profile_.push == PushMode::Press ? "PRESS" : "CLICK") << "\""
    << ",\"hold\":" << profile_.hold_interval_ms
    << ",\"device_kind\":\"" << profile_.device_kind << "\""
    << ",\"game_preset\":\"" << profile_.game_preset << "\""
    << ",\"bits\":[";
  for (size_t i = 0; i < st.bits.size(); ++i) {
    if (i) o << ",";
    o << (st.bits[i] ? "true" : "false");
  }
  o << "],\"keys\":{"
    << "\"power_up\":\"" << profile_.keys.power_up << "\""
    << ",\"power_down\":\"" << profile_.keys.power_down << "\""
    << ",\"power_off\":\"" << profile_.keys.power_off << "\""
    << ",\"brake_up\":\"" << profile_.keys.brake_up << "\""
    << ",\"brake_down\":\"" << profile_.keys.brake_down << "\""
    << ",\"brake_eb\":\"" << profile_.keys.brake_eb << "\""
    << ",\"pedal\":\"" << profile_.keys.pedal << "\""
    << ",\"btn_a\":\"" << profile_.keys.btn_a << "\""
    << ",\"btn_b\":\"" << profile_.keys.btn_b << "\""
    << ",\"btn_c\":\"" << profile_.keys.btn_c << "\""
    << ",\"btn_d\":\"" << profile_.keys.btn_d << "\""
    << ",\"btn_select\":\"" << profile_.keys.btn_select << "\""
    << ",\"btn_start\":\"" << profile_.keys.btn_start << "\""
    << ",\"hat_u\":\"" << profile_.keys.hat_u << "\""
    << ",\"hat_d\":\"" << profile_.keys.hat_d << "\""
    << ",\"hat_l\":\"" << profile_.keys.hat_l << "\""
    << ",\"hat_r\":\"" << profile_.keys.hat_r << "\"";
  o << "}";
  if (toast_pending_) {
    o << ",\"toast\":{\"title\":\"" << toast_title_ << "\",\"body\":\"" << toast_body_ << "\"}";
  }
  if (driver_prompt_) o << ",\"driver_prompt\":true";
  o << "}";
  return o.str();
}

void AppState::ui_tick() {
  ControllerState st;
  Profile p;
  bool clear_toast = false;
  bool clear_prompt = false;
  {
    std::lock_guard lock(mu_);
    st = live_;
    p = profile_;
  }
  bool door = false, vibl = false, vibr = false;
  bool have_fb = false;
  if (mmf_.ok()) {
    if (!p.disable_feedback) {
      if (test_frames_left_ > 0) {
        --test_frames_left_;
        if (test_frames_left_ == 0) {
          mmf_.clear_feedback();
          usb_.send_feedback(false, false, false);
        }
      }
      mmf_.read_feedback(st);
      door = st.door_lamp;
      vibl = st.vib_l;
      vibr = st.vib_r;
      have_fb = true;
      if (st.connected) mmf_.write_notches(st.brake, st.power);
    } else {
      mmf_.clear_feedback();
      usb_.send_feedback(false, false, false);
    }
  }
  // Mirror MMF → physical Type2 (plugin + local test_* both write MMF first)
  if (have_fb && !p.disable_feedback && usb_.has_handle()) {
    usb_.send_feedback(door, vibl, vibr);
  }
  keys_.set_profile(p);
  keys_.tick(st);
  {
    std::lock_guard lock(mu_);
    if (have_fb) {
      live_.door_lamp = door;
      live_.vib_l = vibl;
      live_.vib_r = vibr;
    }
    live_.atc = st.atc;
    live_.speed = st.speed;
    st = live_;
    if (toast_pending_) clear_toast = true;
    if (driver_prompt_) clear_prompt = true;
  }
  if (ui_push_) ui_push_(state_to_json(st));
  if (clear_toast || clear_prompt) {
    std::lock_guard lock(mu_);
    if (clear_toast) toast_pending_ = false;
    if (clear_prompt) driver_prompt_ = false;
  }
}
