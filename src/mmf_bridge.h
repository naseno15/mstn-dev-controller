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

private:
  void* mapping_ = nullptr;
  void* view_ = nullptr;
  static constexpr int kSize = 64;
};
