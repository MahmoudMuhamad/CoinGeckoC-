#include "storage.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using nlohmann::json;
namespace fs = std::filesystem;

static fs::path config_dir() {
    return fs::current_path() / "config";
}

std::string Storage::ensure_config_dir() {
    auto dir = config_dir();
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

static fs::path favorites_path() { return config_dir() / "favorites.json"; }
static fs::path settings_path()  { return config_dir() / "settings.json"; }

bool Storage::save_favorites(const AppState& st, std::string& err) {
    err.clear();
    ensure_config_dir();

    json j;
    j["favorites"] = st.favorites;

    std::ofstream ofs(favorites_path(), std::ios::binary);
    if (!ofs) { err = "Failed to open favorites file for writing"; return false; }
    ofs << j.dump(2);
    return true;
}

bool Storage::load_favorites(AppState& st, std::string& err) {
    err.clear();
    ensure_config_dir();

    std::ifstream ifs(favorites_path(), std::ios::binary);
    if (!ifs) {
        // not an error on first run
        return true;
    }

    json j;
    try { ifs >> j; }
    catch (const std::exception& e) { err = std::string("Failed to parse favorites: ") + e.what(); return false; }

    if (j.contains("favorites") && j["favorites"].is_array()) {
        st.favorites.clear();
        for (const auto& v : j["favorites"]) {
            if (v.is_string()) st.favorites.push_back(v.get<std::string>());
        }
    }
    return true;
}

bool Storage::save_settings(const AppState& st, std::string& err) {
    err.clear();
    ensure_config_dir();

    json j;
    j["auto_refresh"] = st.auto_refresh;
    j["refresh_seconds"] = st.refresh_seconds;

    std::ofstream ofs(settings_path(), std::ios::binary);
    if (!ofs) { err = "Failed to open settings file for writing"; return false; }
    ofs << j.dump(2);
    return true;
}

bool Storage::load_settings(AppState& st, std::string& err) {
    err.clear();
    ensure_config_dir();

    std::ifstream ifs(settings_path(), std::ios::binary);
    if (!ifs) {
        return true;
    }

    json j;
    try { ifs >> j; }
    catch (const std::exception& e) { err = std::string("Failed to parse settings: ") + e.what(); return false; }

    if (j.contains("auto_refresh")) st.auto_refresh = j["auto_refresh"].get<bool>();
    if (j.contains("refresh_seconds")) st.refresh_seconds = j["refresh_seconds"].get<int>();
    if (st.refresh_seconds < 1) st.refresh_seconds = 1;
    return true;
}
