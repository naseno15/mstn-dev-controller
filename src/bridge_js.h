#pragma once
#include "app_state.h"
#include <string>

// Parse UI → host command JSON (minimal).
void handle_ui_command(AppState& app, const std::string& json);
