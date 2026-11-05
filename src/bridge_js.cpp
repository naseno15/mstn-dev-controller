#include "bridge_js.h"

#include <commdlg.h>

#pragma comment(lib, "comdlg32.lib")

namespace {
std::string get_str(const std::string& j, const std::string& key) {
  const std::string pat = "\"" + key + "\"";
  auto p = j.find(pat);
  if (p == std::string::npos) return {};
  p = j.find(':', p);
  if (p == std::string::npos) return {};
  p = j.find('"', p);
  if (p == std::string::npos) return {};
  auto e = j.find('"', p + 1);
  if (e == std::string::npos) return {};
  return j.substr(p + 1, e - p - 1);
}

double get_num(const std::string& j, const std::string& key, double def) {
  const std::string pat = "\"" + key + "\"";
  auto p = j.find(pat);
  if (p == std::string::npos) return def;
  p = j.find(':', p);
  if (p == std::string::npos) return def;
  ++p;
  while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) ++p;
  try {
    return std::stod(j.substr(p));
  } catch (...) {
    return def;
  }
}

bool get_bool(const std::string& j, const std::string& key, bool def) {
  const std::string pat = "\"" + key + "\"";
  auto p = j.find(pat);
  if (p == std::string::npos) return def;
  auto t = j.find("true", p);
  auto f = j.find("false", p);
  if (t != std::string::npos && (f == std::string::npos || t < f)) return true;
  if (f != std::string::npos) return false;
  return def;
}

std::wstring open_save_dialog(HWND owner, bool save) {
  wchar_t file[MAX_PATH] = L"profile.json";
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = L"JSON Profile (*.json)\0*.json\0All\0*.*\0";
  if (save) {
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) return file;
  } else {
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) return file;
  }
  return {};
}
} // namespace

void handle_ui_command(AppState& app, const std::string& json) {
  const std::string cmd = get_str(json, "cmd");
  std::string err;
  if (cmd == "start_usb") {
    app.start_usb();
  } else if (cmd == "stop_usb") {
    app.stop_usb();
  } else if (cmd == "save" || cmd == "export") {
    auto path = open_save_dialog(nullptr, true);
    if (!path.empty()) app.save_profile_path(path, err);
    else app.save_profile_path(app.default_profile_path(), err);
  } else if (cmd == "save_preset") {
    app.save_game_preset(get_str(json, "game"), get_str(json, "device"), err);
  } else if (cmd == "load") {
    auto path = open_save_dialog(nullptr, false);
    if (!path.empty()) app.load_profile_path(path, err);
  } else if (cmd == "set_output") {
    app.apply_output_settings(
        get_bool(json, "enabled", true),
        get_str(json, "push"),
        get_num(json, "hold", 1.0),
        get_bool(json, "disable_feedback", false),
        get_str(json, "device_kind"),
        get_str(json, "game_preset"),
        static_cast<int>(get_num(json, "overshoot", 0)));
  } else if (cmd == "set_key") {
    app.apply_keymap_field(get_str(json, "field"), get_str(json, "value"));
  } else if (cmd == "test_door") {
    app.test_door();
  } else if (cmd == "test_vib") {
    app.test_vib();
  } else if (cmd == "test_vib_on") {
    app.test_vib_on();
  } else if (cmd == "test_vib_off") {
    app.test_vib_off();
  } else if (cmd == "test_vib_set") {
    app.test_vib_set(get_bool(json, "left", false), get_bool(json, "right", false));
  } else if (cmd == "test_off") {
    app.test_off();
  } else if (cmd == "probe_driver") {
    app.probe_driver();
  } else if (cmd == "open_zadig") {
    app.open_zadig();
  } else if (cmd == "reload_default") {
    app.load_profile_path(app.default_profile_path(), err);
  }
}
