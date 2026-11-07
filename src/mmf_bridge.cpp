#include "mmf_bridge.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {
// Flat numeric/bool fields of the telemetry JSON; NAN when absent.
double json_num(const std::string& j, const char* key) {
  const auto p = j.find("\"" + std::string(key) + "\":");
  if (p == std::string::npos) return NAN;
  const char* s = j.c_str() + p + std::strlen(key) + 3;
  if (!std::strncmp(s, "true", 4)) return 1;
  if (!std::strncmp(s, "false", 5)) return 0;
  char* end = nullptr;
  const double v = std::strtod(s, &end);
  return end == s ? NAN : v;
}
} // namespace
#endif

MmfBridge::MmfBridge() = default;
MmfBridge::~MmfBridge() { close(); }

bool MmfBridge::open() {
  close();
#ifdef _WIN32
  HANDLE h = CreateFileMappingW(
      INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, kSize, L"denconvMMF");
  if (!h) return false;
  void* v = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, kSize);
  if (!v) {
    CloseHandle(h);
    return false;
  }
  mapping_ = h;
  view_ = v;
  return true;
#else
  view_ = local_;
  return true;
#endif
}

#ifdef __APPLE__
// Plays DenconvFeedback's role: vehicle notch counts (18–20), speed (16–17) and the door pilot lamp
// (10, lit = all doors closed) from the BveLive telemetry plugin, ~10 Hz like its writer.
// ponytail: no ATC or rumble source in openBVE telemetry; those bytes stay as the Test buttons set them.
void MmfBridge::sync_telemetry() {
  using namespace std::chrono;
  const int64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  if (now - last_poll_ms_ < 100) return;
  last_poll_ms_ = now;

  std::string j;
  if (const char* home = std::getenv("HOME")) {
    std::ifstream f(std::string(home) + "/Documents/BveTs/Settings/live_telemetry.json");
    std::stringstream ss;
    ss << f.rdbuf();
    j = ss.str();
  }
  const double ts = json_num(j, "ts");
  // Stale file = openBVE quit/crashed without its Unload write.
  const bool live = json_num(j, "ok") == 1 && std::isfinite(ts) && now - ts < 3000;
  const double pmax = json_num(j, "power_max"), bmax = json_num(j, "brake_max");
  const double kmh = json_num(j, "speed_kmh"), closed = json_num(j, "doors_closed");

  auto* b = local_;
  if (live && pmax > 0 && bmax > 0) {
    b[18] = static_cast<uint8_t>(pmax);
    b[19] = static_cast<uint8_t>(bmax + 1); // EB index sits one past the last service notch
    b[20] = 0xA5;
  } else {
    b[20] = 0;
  }
  const int spd = live && std::isfinite(kmh) ? static_cast<int>(std::lround(std::fabs(kmh))) : 0;
  b[16] = static_cast<uint8_t>(spd & 0xFF);
  b[17] = static_cast<uint8_t>(spd >> 8);

  // Only on change, so a Test-button lamp stays until the game moves the doors.
  const int doors = live && std::isfinite(closed) ? static_cast<int>(closed) : -1;
  if (doors != last_doors_closed_) {
    b[10] = doors == 1 ? 1 : 0;
    last_doors_closed_ = doors;
  }
}
#endif

void MmfBridge::close() {
#ifdef _WIN32
  if (view_) {
    UnmapViewOfFile(view_);
    view_ = nullptr;
  }
  if (mapping_) {
    CloseHandle(static_cast<HANDLE>(mapping_));
    mapping_ = nullptr;
  }
#else
  view_ = nullptr;
#endif
}

void MmfBridge::write_notches(int brake, int power) {
  if (!view_) return;
  auto* b = static_cast<uint8_t*>(view_);
  b[0] = static_cast<uint8_t>(brake < 0 ? 0 : (brake > 10 ? 10 : brake));
  b[1] = static_cast<uint8_t>(power < 0 ? 0 : (power > 13 ? 13 : power));
  b[2] = 58;
  b[3] = 0;
}

void MmfBridge::write_feedback(bool door, bool vib_l, bool vib_r) {
  if (!view_) return;
  auto* b = static_cast<uint8_t*>(view_);
  b[10] = door ? 1 : 0;
  b[11] = vib_l ? 1 : 0;
  b[12] = vib_r ? 1 : 0;
}

void MmfBridge::clear_feedback() { write_feedback(false, false, false); }

void MmfBridge::read_feedback(ControllerState& st) {
  if (!view_) return;
#ifdef __APPLE__
  sync_telemetry();
#endif
  const auto* b = static_cast<const uint8_t*>(view_);
  st.door_lamp = b[10] == 1;
  st.vib_l = b[11] == 1;
  st.vib_r = b[12] == 1;
  st.atc = b[14] + b[15] * 256;
  st.speed = b[16] + b[17] * 256;
  if (st.atc > 999) st.atc = 0;
  if (st.speed > 999) st.speed = 0;
  read_vehicle_notches(st);
}

void MmfBridge::read_vehicle_notches(ControllerState& st) {
  if (!view_) return;
#ifdef __APPLE__
  sync_telemetry();
#endif
  const auto* b = static_cast<const uint8_t*>(view_);
  // 0xA5 = plugin wrote PowerNotchCount / EmergencyBrakeNotch this scenario
  if (b[20] != 0xA5) {
    st.veh_power_max = 0;
    st.veh_brake_max = 0;
    return;
  }
  st.veh_power_max = b[18];
  st.veh_brake_max = b[19];
}
