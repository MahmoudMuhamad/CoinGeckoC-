# Design — CoinGecko Edition

## Data Model
- `CoinQuote { coin_id, usd, usd_24h_change, last_updated }`

## State
- `unordered_map<string, CoinQuote> cache_by_coin`
- `vector<string> favorites`
- Protected by `mutex data_mutex`

## Threading
- Background refresh thread runs every `refresh_seconds` when `auto_refresh` is enabled.
- Controlled by `atomic<bool> running` and guarded updates using the mutex.

## Persistence
- Favorites/settings stored in JSON files using `fstream`
- `filesystem` ensures config directory exists.
