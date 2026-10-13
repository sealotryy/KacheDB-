#include "kv_store.h"

#include <limits>

KvStore::KvStore(Clock::time_point (*now)()) : now_(now) {}

void KvStore::set(const std::string& key, const std::string& value) {
    // A plain SET overwrites both the value and any previous expiration,
    // matching Redis semantics.
    data_[key] = Entry{value, std::nullopt};
}

// Converts whole seconds to the clock duration, rejecting any value that
// overflows before reaching set_ex(). A duration that passes here is still
// checked again when added to now_() in case the clock itself is close to
// its maximum representable time.
bool KvStore::set_ex(
    const std::string& key,
    const std::string& value,
    Clock::duration ttl
) {
    if (ttl < Clock::duration::zero()) {
        return false;
    }

    const Clock::time_point now = now_();

    // Compare against the largest deadline the clock can represent so the
    // addition now + ttl never overflows. Writing this as now > max - ttl
    // instead avoids a second subtraction that itself overflows when now is
    // negative (e.g. before the epoch).
    if (now > Clock::time_point::max() - ttl) {
        return false;
    }

    data_[key] = Entry{value, now + ttl};
    return true;
}

std::optional<std::string> KvStore::get(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    if (erase_if_expired(it)) {
        return std::nullopt;
    }

    return it->second.value;
}

bool KvStore::del(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return false;
    }

    // An expired key counts as missing, not as a successful delete.
    if (erase_if_expired(it)) {
        return false;
    }

    // erase_if_expired returning false leaves the iterator valid because
    // no erase() call ran.
    data_.erase(it);
    return true;
}

std::optional<long> KvStore::ttl(const std::string& key) {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    // Sample the clock once so the expiration check and the remaining-time
    // calculation always agree, even if the real clock crosses the deadline
    // between the two reads.
    const Clock::time_point now = now_();

    const std::optional<Clock::time_point>& deadline = it->second.expires_at;

    if (deadline.has_value() && now >= *deadline) {
        data_.erase(it);
        return std::nullopt;
    }

    if (!deadline.has_value()) {
        return -1;
    }

    const Clock::duration remaining = *deadline - now;
    const long seconds =
        std::chrono::duration_cast<std::chrono::seconds>(remaining).count();

    // While less than a full second remains TTL reports 0, and the key is
    // still readable until now() reaches expires_at.
    return seconds;
}

bool KvStore::erase_if_expired(
    std::unordered_map<std::string, Entry>::iterator it
) {
    const std::optional<Clock::time_point>& deadline = it->second.expires_at;

    if (!deadline.has_value()) {
        return false;
    }

    if (now_() < *deadline) {
        return false;
    }

    data_.erase(it);
    return true;
}
