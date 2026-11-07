#pragma once
#include "controller_state.h"
#include <cstdint>

class MmfBridge {
public:
  MmfBridge();
  ~MmfBridge();

  bool open();
  void close();
  bool ok() const { return view_ != nullptr; }

  void write_notches(int brake, int power);
  void write_feedback(bool door, bool vib_l, bool vib_r);
  void clear_feedback();
  void read_feedback(ControllerState& st);
  // Bytes 18–20 written by DenconvFeedback (vehicle NotchInfo).
  void read_vehicle_notches(ControllerState& st);

private:
  static constexpr int kSize = 64;
  void* mapping_ = nullptr;
  void* view_ = nullptr;
#ifdef __APPLE__
  // macOS has no denconvMMF: view_ points at local_, filled from openBVE's live_telemetry.json.
  void sync_telemetry();
  uint8_t local_[kSize]{};
  int last_doors_closed_ = -1; // -1 = no live scenario
  int64_t last_poll_ms_ = 0;
#endif
};
