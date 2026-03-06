#include "coingecko_client.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

using nlohmann::json;

std::optional<CoinQuote> CoinGeckoClient::fetch_simple_price(const std::string& coin_id, std::string& err_out) const {
    err_out.clear();

    const std::string id = coin_id;
    if (id.empty()) {
        err_out = "Coin id is empty.";
        return std::nullopt;
    }

    // CoinGecko endpoint:
    // GET https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd&include_24hr_change=true
    httplib::SSLClient cli("api.coingecko.com", 443);
    cli.set_follow_location(true);

    std::string path = "/api/v3/simple/price?ids=" + id + "&vs_currencies=usd&include_24hr_change=true";

    auto res = cli.Get(path.c_str());
    if (!res) {
        err_out = "Network error: request failed (no response).";
        return std::nullopt;
    }
    

    if (res->status == 429) {
        err_out = "Rate limit (429): slow down auto-refresh (try 20-30s).";
        return std::nullopt;
    }
    if (res->status != 200) {
        err_out = "HTTP error: status=" + std::to_string(res->status) + " body=" + res->body.substr(0, 300);
        return std::nullopt;
    }

    json j;
    try {
        j = json::parse(res->body);
    } catch (const std::exception& e) {
        err_out = std::string("JSON parse error: ") + e.what();
        return std::nullopt;
    }

    if (!j.contains(id)) {
        err_out = "Coin id not found. Try: bitcoin, ethereum, solana, dogecoin, cardano ...";
        return std::nullopt;
    }

    const auto& o = j[id];
    CoinQuote q;
    q.coin_id = id;

    if (o.contains("usd") && o["usd"].is_number()) q.usd = o["usd"].get<double>();
    if (o.contains("usd_24h_change") && o["usd_24h_change"].is_number()) q.usd_24h_change = o["usd_24h_change"].get<double>();

    return q;
}
