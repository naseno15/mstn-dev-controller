#undef NDEBUG
#include "mmf_bridge.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static void write_telemetry(const fs::path& file, const std::string& body) {
  std::ofstream(file) << "\xEF\xBB\xBF" << body; // plugin writes a UTF-8 BOM
  std::this_thread::sleep_for(std::chrono::milliseconds(120)); // past the 10 Hz poll gate
}

int main() {
  const fs::path home = fs::temp_directory_path() / "mstn_check_home";
  const fs::path dir = home / "Documents/BveTs/Settings";
  fs::create_directories(dir);
  setenv("HOME", home.c_str(), 1);
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch()).count();

  MmfBridge m;
  assert(m.open());
  ControllerState st;

  // Live openBVE train: P4 / B7 notched, doors closed, 82.6 km/h
  write_telemetry(dir / "live_telemetry.json",
                  "{\"ok\":true,\"source\":\"openbve\",\"ts\":" + std::to_string(now) +
                      ",\"speed_kmh\":82.60,\"power_max\":4,\"brake_max\":7,\"doors_closed\":true}");
  m.read_feedback(st);
  assert(st.veh_power_max == 4);
  assert(st.veh_brake_max == 8); // EB = B7 + 1
  assert(st.door_lamp);
  assert(st.speed == 83);

  // Test button lamp survives while the game's door state is unchanged
  m.write_feedback(false, false, false);
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  m.read_feedback(st);
  assert(!st.door_lamp);

  // Scenario closed → unknown vehicle (1:1 mapping), lamp off
  write_telemetry(dir / "live_telemetry.json", "{\"ok\":false,\"reason\":\"scenario_closed\"}");
  m.read_feedback(st);
  assert(st.veh_power_max == 0 && st.veh_brake_max == 0);
  assert(!st.door_lamp && st.speed == 0);

  // Stale file (openBVE crashed) counts as closed
  write_telemetry(dir / "live_telemetry.json",
                  "{\"ok\":true,\"ts\":" + std::to_string(now - 10000) + ",\"power_max\":4,\"brake_max\":7}");
  m.read_feedback(st);
  assert(st.veh_power_max == 0);

  fs::remove_all(home);
  std::printf("check_mmf_telemetry: ok\n");
  return 0;
}
