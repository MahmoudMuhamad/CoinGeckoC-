#pragma once
#include "app_state.h"
#include <string>

namespace Storage {
    // Ensures config directory exists; returns absolute path.
    std::string ensure_config_dir();

    bool save_favorites(const AppState& st, std::string& err);
    bool load_favorites(AppState& st, std::string& err);

    bool save_settings(const AppState& st, std::string& err);
    bool load_settings(AppState& st, std::string& err);
}
