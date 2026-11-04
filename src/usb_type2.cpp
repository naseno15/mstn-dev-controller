#include "usb_type2.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>
#include <initguid.h>
#include <usbiodef.h>

#include <chrono>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "winusb.lib")
#pragma comment(lib, "setupapi.lib")

namespace {
void emit_status(UsbType2::StatusFn& fn, std::mutex& mu, const std::string& s) {
  UsbType2::StatusFn cb;
  {
    std::lock_guard lock(mu);
    cb = fn;
  }
  if (cb) cb(s);
}

std::wstring lower_copy(std::wstring s) {
  for (auto& ch : s) if (ch >= L'A' && ch <= L'Z') ch = ch - L'A' + L'a';
  return s;
}
} // namespace

UsbType2::UsbType2() = default;
UsbType2::~UsbType2() { stop(); }

void UsbType2::set_needle(std::wstring needle) {
  std::lock_guard lock(cb_mu_);
  needle_ = std::move(needle);
}

void UsbType2::set_on_packet(PacketFn fn) {
  std::lock_guard lock(cb_mu_);
  on_packet_ = std::move(fn);
}

void UsbType2::set_on_status(StatusFn fn) {
  std::lock_guard lock(cb_mu_);
  on_status_ = std::move(fn);
}

void UsbType2::start() {
  if (running_.exchange(true)) return;
  stop_ = false;
  th_ = std::thread([this] { thread_main(); });
}

void UsbType2::stop() {
  stop_ = true;
  if (th_.joinable()) th_.join();
  running_ = false;
}

std::wstring UsbType2::find_device_path(const std::wstring& needle) {
  GUID guid = GUID_DEVINTERFACE_USB_DEVICE;
  HDEVINFO h = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (h == INVALID_HANDLE_VALUE) return {};

  const std::wstring want = lower_copy(needle);
  std::wstring found;
  for (DWORD i = 0;; ++i) {
    SP_DEVICE_INTERFACE_DATA did{};
    did.cbSize = sizeof(did);
    if (!SetupDiEnumDeviceInterfaces(h, nullptr, &guid, i, &did)) break;

    DWORD req = 0;
    SetupDiGetDeviceInterfaceDetailW(h, &did, nullptr, 0, &req, nullptr);
    if (req == 0) continue;

    auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(malloc(req));
    if (!detail) continue;
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (SetupDiGetDeviceInterfaceDetailW(h, &did, detail, req, &req, nullptr)) {
      std::wstring lower = lower_copy(detail->DevicePath);
      if (lower.find(want) != std::wstring::npos) found = detail->DevicePath;
    }
    free(detail);
    if (!found.empty()) break;
  }
  SetupDiDestroyDeviceInfoList(h);
  return found;
}

std::string UsbType2::probe_driver(const std::wstring& needle) {
  const std::wstring path = find_device_path(needle);
  if (path.empty()) {
    // Also look for any 0ae4 presence via wider scan of USB devices string in path list
    const std::wstring any = find_device_path(L"vid_0ae4");
    if (any.empty()) return "missing";
    return "needs_winusb";
  }

  HANDLE file = CreateFileW(
      path.c_str(),
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
      nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION) return "busy";
    return "needs_winusb";
  }

  WINUSB_INTERFACE_HANDLE winusb = nullptr;
  const BOOL ok = WinUsb_Initialize(file, &winusb);
  if (winusb) WinUsb_Free(winusb);
  CloseHandle(file);
  return ok ? "ok" : "needs_winusb";
}

void UsbType2::thread_main() {
  std::wstring needle;
  {
    std::lock_guard lock(cb_mu_);
    needle = needle_;
  }
  emit_status(on_status_, cb_mu_, "searching device");
  const std::wstring path = find_device_path(needle);
  if (path.empty()) {
    const auto st = probe_driver(needle);
    if (st == "needs_winusb")
      emit_status(on_status_, cb_mu_, "needs_winusb");
    else
      emit_status(on_status_, cb_mu_, "device not found");
    running_ = false;
    return;
  }

  HANDLE file = CreateFileW(
      path.c_str(),
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
      nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    emit_status(on_status_, cb_mu_, "CreateFile failed (close denconv?) err=" + std::to_string(GetLastError()));
    running_ = false;
    return;
  }

  WINUSB_INTERFACE_HANDLE winusb = nullptr;
  if (!WinUsb_Initialize(file, &winusb)) {
    emit_status(on_status_, cb_mu_, "needs_winusb");
    CloseHandle(file);
    running_ = false;
    return;
  }

  UCHAR use_pipe = 0x81;
  for (UCHAR i = 0; i < 8; ++i) {
    WINUSB_PIPE_INFORMATION pi{};
    if (!WinUsb_QueryPipe(winusb, 0, i, &pi)) break;
    if (pi.PipeId & 0x80) {
      use_pipe = pi.PipeId;
      break;
    }
  }

  char pbuf[8];
  snprintf(pbuf, sizeof(pbuf), "%02X", use_pipe);
  emit_status(on_status_, cb_mu_, std::string("connected pipe=0x") + pbuf);

  uint8_t buf[64]{};
  int reads = 0;
  auto t0 = std::chrono::steady_clock::now();

  while (!stop_) {
    ULONG transferred = 0;
    if (!WinUsb_ReadPipe(winusb, use_pipe, buf, 8, &transferred, nullptr) || transferred == 0) {
      Sleep(5);
      continue;
    }
    ++reads;
    const auto now = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(now - t0).count();
    const double hz = reads / (secs > 0.001 ? secs : 0.001);

    PacketFn copy;
    {
      std::lock_guard lock(cb_mu_);
      copy = on_packet_;
    }
    if (copy) copy(buf, static_cast<int>(transferred), hz);
  }

  WinUsb_Free(winusb);
  CloseHandle(file);
  emit_status(on_status_, cb_mu_, "disconnected");
  running_ = false;
}
