#pragma once
#include "models.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>

struct AppState {
    // shared data (guard with data_mutex)
    std::unordered_map<std::string, CoinQuote> cache_by_coin;
    std::vector<std::string> favorites;
    std::string last_error;

    // settings
    bool auto_refresh = true;
    int refresh_seconds = 30;

    // synchronization
    std::mutex data_mutex;
};
