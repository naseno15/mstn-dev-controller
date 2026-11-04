#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include "app_window.h"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) return 1;

  AppWindow app(inst);
  if (!app.create()) {
    MessageBoxW(nullptr, L"Failed to create window", L"mstnDevController", MB_ICONERROR);
    CoUninitialize();
    return 2;
  }
  const int code = app.run();
  CoUninitialize();
  return code;
}
