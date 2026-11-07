#undef NDEBUG
#define KeyMap CarbonKeyMap
#include <Carbon/Carbon.h>
#undef KeyMap

#include "profile.h"

#include <cassert>
#include <cstdio>
#include <string>

int main() {
  // Every key name the UI can bind must reach a real Mac key.
  const char* names[] = {"SPACE", "RETURN", "UP", "DOWN", "LEFT", "RIGHT", "INSERT", "DELETE",
                         "HOME", "END", "PRIOR", "NEXT", "TAB", "ESCAPE", "BACK", "OEM_PERIOD",
                         "OEM_COMMA", "OEM_2", "OEM_1", "OEM_4", "OEM_6", "OEM_5", "OEM_7",
                         "OEM_MINUS", "OEM_PLUS", "ADD"};
  for (const char* n : names) assert(mac_keycode(vk_from_name(n)) >= 0);
  for (char c = 'A'; c <= 'Z'; ++c) assert(mac_keycode(vk_from_name(std::string(1, c))) >= 0);
  for (char c = '0'; c <= '9'; ++c) assert(mac_keycode(vk_from_name(std::string(1, c))) >= 0);

  // BVE6 preset keys
  assert(mac_keycode(vk_from_name("Z")) == kVK_ANSI_Z);
  assert(mac_keycode(vk_from_name("A")) == kVK_ANSI_A); // kVK 0 is a real key, not "unbound"
  assert(mac_keycode(vk_from_name("OEM_PERIOD")) == kVK_ANSI_Period);
  assert(mac_keycode(vk_from_name("OEM_COMMA")) == kVK_ANSI_Comma);
  assert(mac_keycode(vk_from_name("OEM_2")) == kVK_ANSI_Slash);
  assert(mac_keycode(vk_from_name("SPACE")) == kVK_Space);
  assert(mac_keycode(0) == -1);

  std::printf("check_mac_keys: ok\n");
  return 0;
}
