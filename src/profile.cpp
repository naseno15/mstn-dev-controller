#ifdef __APPLE__
// kVK_* constants; Carbon's own `KeyMap` typedef would collide with ours.
#define KeyMap CarbonKeyMap
#include <Carbon/Carbon.h>
#undef KeyMap
#endif

#include "profile.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace {

std::string json_get_string(const std::string& j, const std::string& key, const std::string& def) {
  const std::string pat = "\"" + key + "\"";
  auto p = j.find(pat);
  if (p == std::string::npos) return def;
  p = j.find(':', p);
  if (p == std::string::npos) return def;
  p = j.find('"', p);
  if (p == std::string::npos) return def;
  auto e = j.find('"', p + 1);
  if (e == std::string::npos) return def;
  return j.substr(p + 1, e - p - 1);
}

double json_get_number(const std::string& j, const std::string& key, double def) {
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

bool json_get_bool(const std::string& j, const std::string& key, bool def) {
  const std::string pat = "\"" + key + "\"";
  auto p = j.find(pat);
  if (p == std::string::npos) return def;
  p = j.find(':', p);
  if (p == std::string::npos) return def;
  if (j.find("true", p) < j.find(',', p) && j.find("true", p) != std::string::npos) return true;
  if (j.find("false", p) < j.find(',', p) && j.find("false", p) != std::string::npos) return false;
  return def;
}

std::vector<int> json_get_int_array(const std::string& j, const std::string& key) {
  std::vector<int> out;
  const std::string pat = "\"" + key + "\"";
  auto p = j.find(pat);
  if (p == std::string::npos) return out;
  p = j.find('[', p);
  if (p == std::string::npos) return out;
  auto e = j.find(']', p);
  if (e == std::string::npos) return out;
  std::string body = j.substr(p + 1, e - p - 1);
  std::stringstream ss(body);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    try {
      // allow 0xNN
      tok.erase(0, tok.find_first_not_of(" \t\r\n"));
      if (tok.rfind("0x", 0) == 0 || tok.rfind("0X", 0) == 0)
        out.push_back(std::stoi(tok, nullptr, 16));
      else
        out.push_back(std::stoi(tok));
    } catch (...) {
    }
  }
  return out;
}

std::string esc(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') o.push_back('\\');
    o.push_back(c);
  }
  return o;
}

} // namespace

Profile default_bve6_profile() {
  Profile p;
  p.name = "bve6-type2";
  p.device_kind = "type2";
  p.game_preset = "bve6";
  p.hold_interval_ms = 1.0;
  p.notch_overshoot = 0;
  p.push = PushMode::Click;
  p.disable_feedback = false;
  p.keys = KeyMap{};
  p.calib = default_type2_calib();
  return p;
}

std::wstring device_vidpid_needle(const std::string& kind) {
  if (kind == "shinkansen") return L"vid_0ae4&pid_0005";
  if (kind == "ryojou") return L"vid_0ae4&pid_0007";
  if (kind == "mtc") return L"vid_0ae4&pid_0101";
  if (kind == "keikyu") return L"vid_1c06&pid_77a7";
  // type2 + default PS2USB family
  return L"vid_0ae4&pid_0004";
}

WORD vk_from_name(const std::string& name) {
  if (name.empty()) return 0;
  std::string n = name;
  for (auto& c : n) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');

  if (n == "SPACE") return VK_SPACE;
  if (n == "RETURN" || n == "ENTER") return VK_RETURN;
  if (n == "UP") return VK_UP;
  if (n == "DOWN") return VK_DOWN;
  if (n == "LEFT") return VK_LEFT;
  if (n == "RIGHT") return VK_RIGHT;
  if (n == "INSERT") return VK_INSERT;
  if (n == "DELETE") return VK_DELETE;
  if (n == "HOME") return VK_HOME;
  if (n == "END") return VK_END;
  if (n == "PRIOR" || n == "PAGEUP" || n == "PGUP") return VK_PRIOR;
  if (n == "NEXT" || n == "PAGEDOWN" || n == "PGDN") return VK_NEXT;
  if (n == "TAB") return VK_TAB;
  if (n == "ESCAPE" || n == "ESC") return VK_ESCAPE;
  if (n == "BACK" || n == "BACKSPACE") return VK_BACK;
  if (n == "OEM_PERIOD" || n == ".") return VK_OEM_PERIOD;
  if (n == "OEM_COMMA" || n == ",") return VK_OEM_COMMA;
  if (n == "OEM_2" || n == "/") return VK_OEM_2;
  if (n == "OEM_1" || n == ";") return VK_OEM_1;
  if (n == "OEM_4" || n == "[") return VK_OEM_4;
  if (n == "OEM_6" || n == "]") return VK_OEM_6;
  if (n == "OEM_5" || n == "\\") return VK_OEM_5;
  if (n == "OEM_7" || n == "'") return VK_OEM_7;
  if (n == "OEM_MINUS" || n == "-") return VK_OEM_MINUS;
  if (n == "OEM_PLUS" || n == "=") return VK_OEM_PLUS;
  if (n == "ADD" || n == "NUM+") return VK_ADD;
  if (n.size() == 1 && n[0] >= 'A' && n[0] <= 'Z') return static_cast<WORD>(n[0]);
  if (n.size() == 1 && n[0] >= '0' && n[0] <= '9') return static_cast<WORD>(n[0]);
  return 0;
}

#ifdef __APPLE__
int mac_keycode(WORD vk) {
  static const int letters[26] = {
      kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E, kVK_ANSI_F, kVK_ANSI_G,
      kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N,
      kVK_ANSI_O, kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T, kVK_ANSI_U,
      kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z};
  static const int digits[10] = {kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
                                 kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};
  if (vk >= 'A' && vk <= 'Z') return letters[vk - 'A'];
  if (vk >= '0' && vk <= '9') return digits[vk - '0'];
  switch (vk) {
  case VK_SPACE: return kVK_Space;
  case VK_RETURN: return kVK_Return;
  case VK_UP: return kVK_UpArrow;
  case VK_DOWN: return kVK_DownArrow;
  case VK_LEFT: return kVK_LeftArrow;
  case VK_RIGHT: return kVK_RightArrow;
  case VK_INSERT: return kVK_Help; // Mac extended keyboards put Help where Insert is
  case VK_DELETE: return kVK_ForwardDelete;
  case VK_HOME: return kVK_Home;
  case VK_END: return kVK_End;
  case VK_PRIOR: return kVK_PageUp;
  case VK_NEXT: return kVK_PageDown;
  case VK_TAB: return kVK_Tab;
  case VK_ESCAPE: return kVK_Escape;
  case VK_BACK: return kVK_Delete;
  case VK_OEM_PERIOD: return kVK_ANSI_Period;
  case VK_OEM_COMMA: return kVK_ANSI_Comma;
  case VK_OEM_2: return kVK_ANSI_Slash;
  case VK_OEM_1: return kVK_ANSI_Semicolon;
  case VK_OEM_4: return kVK_ANSI_LeftBracket;
  case VK_OEM_6: return kVK_ANSI_RightBracket;
  case VK_OEM_5: return kVK_ANSI_Backslash;
  case VK_OEM_7: return kVK_ANSI_Quote;
  case VK_OEM_MINUS: return kVK_ANSI_Minus;
  case VK_OEM_PLUS: return kVK_ANSI_Equal;
  case VK_ADD: return kVK_ANSI_KeypadPlus;
  default: return -1;
  }
}
#endif

bool load_profile(const std::filesystem::path& path, Profile& out, std::string& err) {
  std::ifstream f(path);
  if (!f) {
    err = "cannot open profile";
    return false;
  }
  std::stringstream buf;
  buf << f.rdbuf();
  const std::string j = buf.str();
  out = default_bve6_profile();
  out.name = json_get_string(j, "name", out.name);
  out.device_kind = json_get_string(j, "device_kind", out.device_kind);
  out.game_preset = json_get_string(j, "game_preset", out.game_preset);
  out.hold_interval_ms = json_get_number(j, "hold_interval_ms", out.hold_interval_ms);
  out.notch_overshoot = static_cast<int>(json_get_number(j, "notch_overshoot", out.notch_overshoot));
  if (out.notch_overshoot < 0) out.notch_overshoot = 0;
  if (out.notch_overshoot > 8) out.notch_overshoot = 8;
  out.output_enabled = json_get_bool(j, "output_enabled", out.output_enabled);
  out.disable_feedback = json_get_bool(j, "disable_feedback", out.disable_feedback);
  const std::string push = json_get_string(j, "push", "CLICK");
  out.push = (push == "PRESS" || push == "press") ? PushMode::Press : PushMode::Click;

  out.keys.power_up = json_get_string(j, "power_up", out.keys.power_up);
  out.keys.power_down = json_get_string(j, "power_down", out.keys.power_down);
  out.keys.power_off = json_get_string(j, "power_off", out.keys.power_off);
  out.keys.brake_up = json_get_string(j, "brake_up", out.keys.brake_up);
  out.keys.brake_down = json_get_string(j, "brake_down", out.keys.brake_down);
  out.keys.brake_eb = json_get_string(j, "brake_eb", out.keys.brake_eb);
  out.keys.pedal = json_get_string(j, "pedal", out.keys.pedal);
  out.keys.btn_a = json_get_string(j, "btn_a", out.keys.btn_a);
  out.keys.btn_b = json_get_string(j, "btn_b", out.keys.btn_b);
  out.keys.btn_c = json_get_string(j, "btn_c", out.keys.btn_c);
  out.keys.btn_d = json_get_string(j, "btn_d", out.keys.btn_d);
  out.keys.btn_select = json_get_string(j, "btn_select", out.keys.btn_select);
  out.keys.btn_start = json_get_string(j, "btn_start", out.keys.btn_start);
  out.keys.hat_u = json_get_string(j, "hat_u", out.keys.hat_u);
  out.keys.hat_d = json_get_string(j, "hat_d", out.keys.hat_d);
  out.keys.hat_l = json_get_string(j, "hat_l", out.keys.hat_l);
  out.keys.hat_r = json_get_string(j, "hat_r", out.keys.hat_r);

  auto pr = json_get_int_array(j, "power_raw");
  auto br = json_get_int_array(j, "brake_raw");
  if (!pr.empty()) out.calib.power_raw = std::move(pr);
  if (!br.empty()) out.calib.brake_raw = std::move(br);
  return true;
}

bool save_profile(const std::filesystem::path& path, const Profile& in, std::string& err) {
  std::ofstream f(path);
  if (!f) {
    err = "cannot write profile";
    return false;
  }
  f << "{\n";
  f << "  \"name\": \"" << esc(in.name) << "\",\n";
  f << "  \"device_kind\": \"" << esc(in.device_kind) << "\",\n";
  f << "  \"game_preset\": \"" << esc(in.game_preset) << "\",\n";
  f << "  \"hold_interval_ms\": " << in.hold_interval_ms << ",\n";
  f << "  \"notch_overshoot\": " << in.notch_overshoot << ",\n";
  f << "  \"push\": \"" << (in.push == PushMode::Press ? "PRESS" : "CLICK") << "\",\n";
  f << "  \"output_enabled\": " << (in.output_enabled ? "true" : "false") << ",\n";
  f << "  \"disable_feedback\": " << (in.disable_feedback ? "true" : "false") << ",\n";
  f << "  \"power_up\": \"" << esc(in.keys.power_up) << "\",\n";
  f << "  \"power_down\": \"" << esc(in.keys.power_down) << "\",\n";
  f << "  \"power_off\": \"" << esc(in.keys.power_off) << "\",\n";
  f << "  \"brake_up\": \"" << esc(in.keys.brake_up) << "\",\n";
  f << "  \"brake_down\": \"" << esc(in.keys.brake_down) << "\",\n";
  f << "  \"brake_eb\": \"" << esc(in.keys.brake_eb) << "\",\n";
  f << "  \"pedal\": \"" << esc(in.keys.pedal) << "\",\n";
  f << "  \"btn_a\": \"" << esc(in.keys.btn_a) << "\",\n";
  f << "  \"btn_b\": \"" << esc(in.keys.btn_b) << "\",\n";
  f << "  \"btn_c\": \"" << esc(in.keys.btn_c) << "\",\n";
  f << "  \"btn_d\": \"" << esc(in.keys.btn_d) << "\",\n";
  f << "  \"btn_select\": \"" << esc(in.keys.btn_select) << "\",\n";
  f << "  \"btn_start\": \"" << esc(in.keys.btn_start) << "\",\n";
  f << "  \"hat_u\": \"" << esc(in.keys.hat_u) << "\",\n";
  f << "  \"hat_d\": \"" << esc(in.keys.hat_d) << "\",\n";
  f << "  \"hat_l\": \"" << esc(in.keys.hat_l) << "\",\n";
  f << "  \"hat_r\": \"" << esc(in.keys.hat_r) << "\",\n";
  f << "  \"power_raw\": [";
  for (size_t i = 0; i < in.calib.power_raw.size(); ++i) {
    if (i) f << ", ";
    f << "0x" << std::hex << in.calib.power_raw[i] << std::dec;
  }
  f << "],\n  \"brake_raw\": [";
  for (size_t i = 0; i < in.calib.brake_raw.size(); ++i) {
    if (i) f << ", ";
    f << "0x" << std::hex << in.calib.brake_raw[i] << std::dec;
  }
  f << "]\n}\n";
  return true;
}
