#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

class KvStore {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    explicit KvStore(Clock::time_point (*now)() = Clock::now);

    void set(const std::string& key, const std::string& value);

    // Returns false without modifying the key when the duration overflows
    // the clock's range, leaving any previous value and expiration intact.
    bool set_ex(
        const std::string& key,
        const std::string& value,
        Clock::duration ttl
    );

    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);

    // Returns:
    //   std::nullopt - key is missing or already expired
    //   -1           - key exists with no expiration set
    //   >= 0         - whole seconds remaining (rounded down)
    std::optional<long> ttl(const std::string& key);

private:
    // A key with expires_at is treated as already expired once
    // now() >= expires_at. get()/del()/ttl() remove expired entries lazily.
    struct Entry {
        std::string value;
        std::optional<Clock::time_point> expires_at;
    };

    // Returns true if the entry at this position has expired and has been erased.
    bool erase_if_expired(
        std::unordered_map<std::string, Entry>::iterator it
    );

    std::unordered_map<std::string, Entry> data_;
    Clock::time_point (*now_)();
};
