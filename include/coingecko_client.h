#pragma once
#include "models.h"
#include <optional>
#include <string>

class CoinGeckoClient {
public:
    CoinGeckoClient() = default;

    // Fetch price for a single coin id (no API key required for this endpoint).
    std::optional<CoinQuote> fetch_simple_price(const std::string& coin_id, std::string& err_out) const;
};
