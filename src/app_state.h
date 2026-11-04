#pragma once
#include "controller_state.h"
#include "decode_type2.h"
#include "keyboard_out.h"
#include "mmf_bridge.h"
#include "profile.h"
#include "usb_type2.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <mutex>
#include <string>

class AppState {
public:
  using UiPushFn = std::function<void(const std::string& json)>;

  AppState();
  ~AppState();

  void set_ui_push(UiPushFn fn);
  void set_hwnd(HWND hwnd);

  void start_usb();
  void stop_usb();

  bool load_profile_path(const std::wstring& path, std::string& err);
  bool save_profile_path(const std::wstring& path, std::string& err);
  bool save_game_preset(const std::string& game, const std::string& device, std::string& err);
  // Write active game/device preset to disk (no toast when quiet).
  bool persist_active_profile(bool quiet = true);

  Profile& profile() { return profile_; }
  const Profile& profile() const { return profile_; }

  void apply_output_settings(bool enabled, const std::string& push, double hold_ms,
                             bool disable_fb, const std::string& device_kind,
                             const std::string& game_preset);
  void apply_keymap_field(const std::string& field, const std::string& value);

  void test_door();
  void test_vib();                         // legacy: both on
  void test_vib_on();                      // legacy: both on
  void test_vib_off();                     // both off
  void test_vib_set(bool left, bool right); // absolute L/R (hold UI)
  void test_off();
  void probe_driver();
  void open_zadig();
  void push_toast(const std::string& title, const std::string& body);

  void ui_tick();

  std::wstring exe_dir() const;
  std::wstring default_profile_path() const;
  std::wstring preset_path_for(const std::string& game, const std::string& device) const;

private:
  void on_packet(const uint8_t* data, int len, double hz);
  void on_usb_status(const std::string& s);
  std::string state_to_json(const ControllerState& st) const;

  mutable std::mutex mu_;
  ControllerState live_{};
  Profile profile_{};
  NotchCalib calib_{};
  UsbType2 usb_;
  MmfBridge mmf_;
  KeyboardOut keys_;
  UiPushFn ui_push_;
  HWND hwnd_ = nullptr;
  std::wstring exe_dir_;
  std::string toast_title_;
  std::string toast_body_;
  bool toast_pending_ = false;
  bool driver_prompt_ = false;
  int test_frames_left_ = 0;
};
