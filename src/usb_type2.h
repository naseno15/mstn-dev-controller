#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class UsbType2 {
public:
  using PacketFn = std::function<void(const uint8_t* data, int len, double hz)>;
  using StatusFn = std::function<void(const std::string& status)>;

  UsbType2();
  ~UsbType2();

  void set_needle(std::wstring needle);
  void start();
  void stop();
  bool running() const { return running_.load(); }

  void set_on_packet(PacketFn fn);
  void set_on_status(StatusFn fn);

  // Queue vendor control OUT (door / rumble). Applied on the USB thread between reads.
  // Returns false only when the device handle is not open yet.
  bool send_feedback(bool door, bool vib_l, bool vib_r);
  bool has_handle() const;
  std::string last_fb_error() const;

  // ok | missing | busy | needs_winusb | wrong_driver:<svc> | err:<code>
  static std::string probe_driver(const std::wstring& needle);

private:
  void thread_main();
  void abort_io();
  // Must run on USB thread with no pending ReadPipe. hariko layout: [function, on/off]
  bool control_out_unlocked(void* winusb, uint8_t function, bool on);
  void flush_feedback_unlocked(void* winusb);
  static std::wstring find_device_path(const std::wstring& needle);
  static std::string driver_service_for_path(const std::wstring& path);

  std::atomic<bool> stop_{false};
  std::atomic<bool> running_{false};
  std::thread th_;
  mutable std::mutex cb_mu_;
  mutable std::mutex io_mu_;
  void* file_ = nullptr;   // HANDLE
  void* winusb_ = nullptr; // WINUSB_INTERFACE_HANDLE
  unsigned char pipe_ = 0x81;
  PacketFn on_packet_;
  StatusFn on_status_;
  std::wstring needle_ = L"vid_0ae4&pid_0004";

  // Desired vs applied feedback (USB thread applies)
  std::atomic<bool> fb_dirty_{false};
  bool want_door_ = false;
  bool want_vibl_ = false;
  bool want_vibr_ = false;
  bool have_door_ = false;
  bool have_vibl_ = false;
  bool have_vibr_ = false;
  std::string last_fb_err_;
};
