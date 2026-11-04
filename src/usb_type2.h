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

  // missing | needs_winusb | ok | busy
  static std::string probe_driver(const std::wstring& needle);

private:
  void thread_main();
  static std::wstring find_device_path(const std::wstring& needle);

  std::atomic<bool> stop_{false};
  std::atomic<bool> running_{false};
  std::thread th_;
  std::mutex cb_mu_;
  PacketFn on_packet_;
  StatusFn on_status_;
  std::wstring needle_ = L"vid_0ae4&pid_0004";
};
