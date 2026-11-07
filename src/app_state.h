#pragma once
#include "controller_state.h"
#include "decode_type2.h"
#include "keyboard_out.h"
#include "mmf_bridge.h"
#include "profile.h"
#include "usb_type2.h"

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>

class AppState {
public:
  using UiPushFn = std::function<void(const std::string& json)>;

  AppState();
  ~AppState();

  void set_ui_push(UiPushFn fn);
#ifdef _WIN32
  void set_hwnd(HWND hwnd);
#endif

  void start_usb();
  void stop_usb();

  bool load_profile_path(const std::filesystem::path& path, std::string& err);
  bool save_profile_path(const std::filesystem::path& path, std::string& err);
  bool save_game_preset(const std::string& game, const std::string& device, std::string& err);
  // Write active game/device preset to disk (no toast when quiet).
  bool persist_active_profile(bool quiet = true);

  Profile& profile() { return profile_; }
  const Profile& profile() const { return profile_; }

  void apply_output_settings(bool enabled, const std::string& push, double hold_ms,
                             bool disable_fb, const std::string& device_kind,
                             const std::string& game_preset, int notch_overshoot);
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

  // Windows: folder of the exe. macOS: ~/Library/Application Support/mstnCon (bundle is read-only).
  std::filesystem::path exe_dir() const;
  std::filesystem::path default_profile_path() const;
  std::filesystem::path preset_path_for(const std::string& game, const std::string& device) const;

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
#ifdef _WIN32
  HWND hwnd_ = nullptr;
#endif
  std::filesystem::path exe_dir_;
  std::string toast_title_;
  std::string toast_body_;
  bool toast_pending_ = false;
  bool driver_prompt_ = false;
  int test_frames_left_ = 0;
};
