#pragma once
#include "app_state.h"

#include <wrl.h>
#include <WebView2.h>

#include <string>

class AppWindow {
public:
  explicit AppWindow(HINSTANCE inst);
  ~AppWindow();

  bool create();
  int run();

  HWND hwnd() const { return hwnd_; }

private:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle_msg(UINT msg, WPARAM wp, LPARAM lp);
  void resize_webview();
  void init_webview();
  void post_to_ui(const std::string& json);

  HINSTANCE inst_ = nullptr;
  HWND hwnd_ = nullptr;
  AppState app_;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
  Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
  UINT_PTR timer_id_ = 1;
};
