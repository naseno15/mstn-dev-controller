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
  for (auto& ch : s)
    if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
  return s;
}

std::string narrow_svc(const wchar_t* w) {
  if (!w || !*w) return {};
  char buf[128]{};
  WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf) - 1, nullptr, nullptr);
  for (char* p = buf; *p; ++p)
    if (*p >= 'A' && *p <= 'Z') *p = static_cast<char>(*p - 'A' + 'a');
  return buf;
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

bool UsbType2::has_handle() const {
  std::lock_guard lock(io_mu_);
  return winusb_ != nullptr;
}

std::string UsbType2::last_fb_error() const {
  std::lock_guard lock(io_mu_);
  return last_fb_err_;
}

void UsbType2::abort_io() {
  std::lock_guard lock(io_mu_);
  if (winusb_) {
    WinUsb_AbortPipe(static_cast<WINUSB_INTERFACE_HANDLE>(winusb_), pipe_);
  }
}

void UsbType2::start() {
  // Previous search may have ended (missing device) with running_=false but
  // th_ still joinable — assigning a new thread without join → std::terminate.
  if (th_.joinable()) {
    stop_ = true;
    abort_io();
    th_.join();
  }
  if (running_.exchange(true)) return;
  stop_ = false;
  th_ = std::thread([this] { thread_main(); });
}

void UsbType2::stop() {
  stop_ = true;
  abort_io();
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
    SP_DEVINFO_DATA devinfo{};
    devinfo.cbSize = sizeof(devinfo);
    if (SetupDiGetDeviceInterfaceDetailW(h, &did, detail, req, &req, &devinfo)) {
      std::wstring lower = lower_copy(detail->DevicePath);
      if (lower.find(want) != std::wstring::npos) found = detail->DevicePath;
    }
    free(detail);
    if (!found.empty()) break;
  }
  SetupDiDestroyDeviceInfoList(h);
  return found;
}

std::string UsbType2::driver_service_for_path(const std::wstring& path) {
  GUID guid = GUID_DEVINTERFACE_USB_DEVICE;
  HDEVINFO h = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (h == INVALID_HANDLE_VALUE) return {};

  const std::wstring want = lower_copy(path);
  std::string svc;
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
    SP_DEVINFO_DATA devinfo{};
    devinfo.cbSize = sizeof(devinfo);
    if (SetupDiGetDeviceInterfaceDetailW(h, &did, detail, req, &req, &devinfo)) {
      if (lower_copy(detail->DevicePath) == want) {
        wchar_t buf[128]{};
        if (SetupDiGetDeviceRegistryPropertyW(h, &devinfo, SPDRP_SERVICE, nullptr,
                                              reinterpret_cast<PBYTE>(buf), sizeof(buf), nullptr)) {
          svc = narrow_svc(buf);
        }
        free(detail);
        break;
      }
    }
    free(detail);
  }
  SetupDiDestroyDeviceInfoList(h);
  return svc;
}

std::string UsbType2::probe_driver(const std::wstring& needle) {
  const std::wstring path = find_device_path(needle);
  if (path.empty()) {
    const std::wstring any = find_device_path(L"vid_0ae4");
    if (any.empty()) return "missing";
    return "missing";
  }

  const std::string svc = driver_service_for_path(path);
  if (!svc.empty() && svc != "winusb") return "wrong_driver:" + svc;

  HANDLE file = CreateFileW(
      path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION || err == ERROR_BUSY)
      return "busy";
    if (svc == "winusb") return "busy";
    return "err:" + std::to_string(err);
  }

  WINUSB_INTERFACE_HANDLE winusb = nullptr;
  const BOOL ok = WinUsb_Initialize(file, &winusb);
  const DWORD init_err = ok ? 0 : GetLastError();
  if (winusb) WinUsb_Free(winusb);
  CloseHandle(file);
  if (ok) return "ok";
  if (!svc.empty() && svc != "winusb") return "wrong_driver:" + svc;
  if (svc == "winusb") return "err:" + std::to_string(init_err);
  return "needs_winusb";
}

bool UsbType2::control_out_unlocked(void* winusb, uint8_t function, bool on) {
  // hariko.com/type2.html: YY ZZ → YY=function (01 L / 02 R / 03 door), ZZ=00 off / 01 on
  // (traincontrollerdb lists the reverse; hardware follows hariko / denconv captures)
  UCHAR payload[2] = {function, static_cast<UCHAR>(on ? 0x01 : 0x00)};
  WINUSB_SETUP_PACKET setup{};
  setup.RequestType = 0x41; // Host→device | Vendor | Interface
  setup.Request = 0x09;
  setup.Value = 0x0201;
  setup.Index = 0x0000;
  setup.Length = 2;
  ULONG transferred = 0;
  const BOOL ok = WinUsb_ControlTransfer(static_cast<WINUSB_INTERFACE_HANDLE>(winusb), setup, payload,
                                         2, &transferred, nullptr);
  if (!ok) {
    last_fb_err_ = "ControlTransfer err=" + std::to_string(GetLastError()) +
                   " fn=" + std::to_string(function);
    return false;
  }
  if (transferred != 2) {
    last_fb_err_ = "short write n=" + std::to_string(transferred);
    return false;
  }
  return true;
}

void UsbType2::flush_feedback_unlocked(void* winusb) {
  if (!winusb || !fb_dirty_.load()) return;

  bool door = false, vibl = false, vibr = false;
  {
    // want_* guarded by io_mu_ (caller holds it, or we take a copy under lock from thread)
  }
  door = want_door_;
  vibl = want_vibl_;
  vibr = want_vibr_;
  fb_dirty_ = false;

  // Always push all three — device state is sticky per function
  const bool ok_d = control_out_unlocked(winusb, 0x03, door);
  const bool ok_l = control_out_unlocked(winusb, 0x01, vibl);
  const bool ok_r = control_out_unlocked(winusb, 0x02, vibr);
  if (ok_d && ok_l && ok_r) {
    have_door_ = door;
    have_vibl_ = vibl;
    have_vibr_ = vibr;
    last_fb_err_.clear();
  }
}

bool UsbType2::send_feedback(bool door, bool vib_l, bool vib_r) {
  std::lock_guard lock(io_mu_);
  if (!winusb_) {
    last_fb_err_ = "no handle (Start USB first)";
    return false;
  }
  if (want_door_ == door && want_vibl_ == vib_l && want_vibr_ == vib_r) return true;
  want_door_ = door;
  want_vibl_ = vib_l;
  want_vibr_ = vib_r;
  fb_dirty_ = true;
  return true; // queued — USB thread applies between reads
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
    if (st.rfind("wrong_driver", 0) == 0) emit_status(on_status_, cb_mu_, st);
    else if (st == "needs_winusb") emit_status(on_status_, cb_mu_, "needs_winusb");
    else emit_status(on_status_, cb_mu_, "device not found");
    running_ = false;
    return;
  }

  {
    const std::string svc = driver_service_for_path(path);
    if (!svc.empty() && svc != "winusb") {
      emit_status(on_status_, cb_mu_, "wrong_driver:" + svc);
      running_ = false;
      return;
    }
  }

  HANDLE file = CreateFileW(
      path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    const DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION)
      emit_status(on_status_, cb_mu_, "busy");
    else
      emit_status(on_status_, cb_mu_, "CreateFile failed (close denconv?) err=" + std::to_string(err));
    running_ = false;
    return;
  }

  WINUSB_INTERFACE_HANDLE winusb = nullptr;
  if (!WinUsb_Initialize(file, &winusb)) {
    const std::string svc = driver_service_for_path(path);
    if (!svc.empty() && svc != "winusb")
      emit_status(on_status_, cb_mu_, "wrong_driver:" + svc);
    else
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

  {
    std::lock_guard lock(io_mu_);
    file_ = file;
    winusb_ = winusb;
    pipe_ = use_pipe;
    have_door_ = have_vibl_ = have_vibr_ = false;
    want_door_ = want_vibl_ = want_vibr_ = false;
    fb_dirty_ = false;
    last_fb_err_.clear();
  }

  emit_status(on_status_, cb_mu_, "ready");

  OVERLAPPED ov{};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  uint8_t buf[64]{};
  int reads = 0;
  auto t0 = std::chrono::steady_clock::now();

  while (!stop_) {
    // Apply queued lamp/vib ONLY when no ReadPipe is outstanding.
    {
      std::lock_guard lock(io_mu_);
      flush_feedback_unlocked(winusb);
    }

    ResetEvent(ov.hEvent);
    ULONG transferred = 0;
    BOOL ok = WinUsb_ReadPipe(winusb, use_pipe, buf, 8, &transferred, &ov);
    if (!ok) {
      const DWORD err = GetLastError();
      if (err == ERROR_IO_PENDING) {
        // Short wait so feedback latency stays low while held.
        const DWORD wr = WaitForSingleObject(ov.hEvent, 40);
        if (wr == WAIT_TIMEOUT) {
          WinUsb_AbortPipe(winusb, use_pipe);
          CancelIoEx(file, &ov);
          WaitForSingleObject(ov.hEvent, 100);
          continue; // loop → flush feedback
        }
        if (!GetOverlappedResult(file, &ov, &transferred, FALSE)) {
          if (stop_) break;
          Sleep(5);
          continue;
        }
      } else {
        if (stop_) break;
        Sleep(5);
        continue;
      }
    }
    if (transferred == 0) continue;

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

  if (ov.hEvent) CloseHandle(ov.hEvent);

  {
    std::lock_guard lock(io_mu_);
    if (winusb_) {
      // Force all outputs off (hariko order)
      control_out_unlocked(winusb_, 0x01, false);
      control_out_unlocked(winusb_, 0x02, false);
      control_out_unlocked(winusb_, 0x03, false);
      WinUsb_Free(static_cast<WINUSB_INTERFACE_HANDLE>(winusb_));
      winusb_ = nullptr;
    }
    if (file_) {
      CloseHandle(static_cast<HANDLE>(file_));
      file_ = nullptr;
    }
  }

  emit_status(on_status_, cb_mu_, "disconnected");
  running_ = false;
}
