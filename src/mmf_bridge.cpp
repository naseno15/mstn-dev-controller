#include "mmf_bridge.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

MmfBridge::MmfBridge() = default;
MmfBridge::~MmfBridge() { close(); }

bool MmfBridge::open() {
  close();
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
}

void MmfBridge::close() {
  if (view_) {
    UnmapViewOfFile(view_);
    view_ = nullptr;
  }
  if (mapping_) {
    CloseHandle(static_cast<HANDLE>(mapping_));
    mapping_ = nullptr;
  }
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
