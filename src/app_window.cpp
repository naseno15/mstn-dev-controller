#include "app_window.h"
#include "bridge_js.h"

#include <shlwapi.h>
#include <string>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kClass[] = L"mstnDevControllerWnd";
constexpr UINT WM_APP_POST_UI = WM_APP + 1;
} // namespace

AppWindow::AppWindow(HINSTANCE inst) : inst_(inst) {}

AppWindow::~AppWindow() {
  if (hwnd_) KillTimer(hwnd_, timer_id_);
}

bool AppWindow::create() {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = AppWindow::WndProc;
  wc.hInstance = inst_;
  wc.lpszClassName = kClass;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(inst_, MAKEINTRESOURCEW(1));
  wc.hIconSm = static_cast<HICON>(LoadImageW(inst_, MAKEINTRESOURCEW(1), IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  // Locked ~16:10 client — taller so monitor gauges breathe
  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  RECT wr{0, 0, 1280, 820};
  AdjustWindowRect(&wr, style, FALSE);
  hwnd_ = CreateWindowExW(
      0, kClass, L"mstnCon — Type2",
      style,
      CW_USEDEFAULT, CW_USEDEFAULT,
      wr.right - wr.left, wr.bottom - wr.top,
      nullptr, nullptr, inst_, this);
  if (!hwnd_) return false;

  app_.set_hwnd(hwnd_);
  app_.set_ui_push([this](const std::string& json) {
    // marshal to UI thread
    auto* heap = new std::string(json);
    if (!PostMessageW(hwnd_, WM_APP_POST_UI, 0, reinterpret_cast<LPARAM>(heap)))
      delete heap;
  });

  ShowWindow(hwnd_, SW_SHOW);
  UpdateWindow(hwnd_);
  init_webview();
  SetTimer(hwnd_, timer_id_, 33, nullptr); // ~30 Hz
  app_.start_usb();
  return true;
}

int AppWindow::run() {
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  AppWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    self = static_cast<AppWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  } else {
    self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
  return self->handle_msg(msg, wp, lp);
}

LRESULT AppWindow::handle_msg(UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_GETMINMAXINFO: {
    auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT wr{0, 0, 1280, 820};
    AdjustWindowRect(&wr, style, FALSE);
    const int w = wr.right - wr.left;
    const int h = wr.bottom - wr.top;
    mmi->ptMinTrackSize = {w, h};
    mmi->ptMaxTrackSize = {w, h};
    return 0;
  }
  case WM_SIZE:
    resize_webview();
    return 0;
  case WM_TIMER:
    if (wp == timer_id_) app_.ui_tick();
    return 0;
  case WM_APP_POST_UI: {
    auto* s = reinterpret_cast<std::string*>(lp);
    if (s) {
      post_to_ui(*s);
      delete s;
    }
    return 0;
  }
  case WM_CLOSE:
    app_.stop_usb();
    DestroyWindow(hwnd_);
    return 0;
  case WM_DESTROY:
    KillTimer(hwnd_, timer_id_);
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProcW(hwnd_, msg, wp, lp);
  }
}

void AppWindow::resize_webview() {
  if (!controller_) return;
  RECT rc{};
  GetClientRect(hwnd_, &rc);
  controller_->put_Bounds(rc);
}

void AppWindow::post_to_ui(const std::string& json) {
  if (!webview_) return;
  const std::wstring w(json.begin(), json.end());
  webview_->PostWebMessageAsJson(w.c_str());
}

void AppWindow::init_webview() {
  // User data folder next to exe
  const std::wstring udf = app_.exe_dir() + L"\\webview2-data";
  CreateDirectoryW(udf.c_str(), nullptr);

  CreateCoreWebView2EnvironmentWithOptions(
      nullptr, udf.c_str(), nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(hr) || !env) return hr;
            return env->CreateCoreWebView2Controller(
                hwnd_,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT hr2, ICoreWebView2Controller* ctrl) -> HRESULT {
                      if (FAILED(hr2) || !ctrl) return hr2;
                      controller_ = ctrl;
                      ctrl->get_CoreWebView2(&webview_);
                      resize_webview();
                      ctrl->put_ZoomFactor(1.06);

                      ComPtr<ICoreWebView2Settings> settings;
                      webview_->get_Settings(&settings);
                      if (settings) {
                        settings->put_IsScriptEnabled(TRUE);
                        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                        settings->put_IsWebMessageEnabled(TRUE);
                        settings->put_AreDevToolsEnabled(TRUE);
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_IsStatusBarEnabled(FALSE);
                        settings->put_IsZoomControlEnabled(FALSE);
                      }

                      webview_->add_WebMessageReceived(
                          Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                              [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                LPWSTR msg = nullptr;
                                if (SUCCEEDED(args->TryGetWebMessageAsString(&msg)) && msg) {
                                  int n = WideCharToMultiByte(CP_UTF8, 0, msg, -1, nullptr, 0, nullptr, nullptr);
                                  std::string s(n > 0 ? n - 1 : 0, '\0');
                                  if (n > 1)
                                    WideCharToMultiByte(CP_UTF8, 0, msg, -1, s.data(), n, nullptr, nullptr);
                                  CoTaskMemFree(msg);
                                  handle_ui_command(app_, s);
                                }
                                return S_OK;
                              })
                              .Get(),
                          nullptr);

                      const std::wstring ui = app_.exe_dir() + L"\\ui\\index.html";
                      std::wstring url = L"file:///" + ui;
                      for (auto& c : url) if (c == L'\\') c = L'/';
                      webview_->Navigate(url.c_str());
                      return S_OK;
                    })
                    .Get());
          })
          .Get());
}
