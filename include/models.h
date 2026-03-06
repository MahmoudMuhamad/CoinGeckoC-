#pragma once
#include <string>

struct CoinQuote {
    std::string coin_id;        // e.g. "bitcoin"
    double usd = 0.0;
    double usd_24h_change = 0.0;
    std::string last_updated;   // optional (may be empty)
};
