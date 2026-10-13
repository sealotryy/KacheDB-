#include <cassert>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "kv_store.h"

void test_missing_key_returns_empty() {
    KvStore store;

    std::optional<std::string> value = store.get("missing");

    assert(!value.has_value());
}

void test_set_then_get_returns_value() {
    KvStore store;

    store.set("name", "Alex");

    std::optional<std::string> value = store.get("name");

    assert(value.has_value());
    assert(value.value() == "Alex");
}

void test_set_overwrites_existing_value() {
    KvStore store;

    store.set("name", "Alex");
    store.set("name", "Sam");

    std::optional<std::string> value = store.get("name");

    assert(value.has_value());
    assert(value.value() == "Sam");
}

void test_delete_existing_key_returns_true_and_removes_key() {
    KvStore store;

    store.set("name", "Alex");

    bool was_deleted = store.del("name");

    assert(was_deleted);
    assert(!store.get("name").has_value());
}

void test_delete_missing_key_returns_false() {
    KvStore store;

    bool was_deleted = store.del("missing");

    assert(!was_deleted);
}

// A controllable clock so TTL tests never need real sleeps.
class FakeClock {
public:
    static KvStore::TimePoint time;
    static KvStore::TimePoint now() {
        return time;
    }
};

KvStore::TimePoint FakeClock::time = KvStore::Clock::now();

void test_setex_returns_value_before_expiry() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "abc123", std::chrono::seconds(60));

    assert(store.get("session").value() == "abc123");
}

void test_setex_key_disappears_at_expiry() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "abc123", std::chrono::seconds(60));

    FakeClock::time += std::chrono::seconds(60);

    // now() == expires_at, so the key has already expired.
    assert(!store.get("session").has_value());
}

void test_ttl_reports_whole_seconds_remaining() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "abc123", std::chrono::seconds(60));

    FakeClock::time += std::chrono::milliseconds(1500);

    // 58.5 seconds left rounds down to 58.
    assert(store.ttl("session").value() == 58);
}

void test_ttl_zero_before_one_second_remains() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "abc123", std::chrono::seconds(60));

    FakeClock::time += std::chrono::milliseconds(59999);

    assert(store.get("session").value() == "abc123");
    assert(store.ttl("session").value() == 0);
}

void test_ttl_is_minus_one_without_expiration() {
    KvStore store(&FakeClock::now);

    store.set("name", "Alex");

    assert(store.ttl("name").value() == -1);
}

void test_ttl_is_minus_two_for_missing_key() {
    KvStore store(&FakeClock::now);

    assert(!store.ttl("missing").has_value());
}

void test_ttl_is_minus_two_after_expiry() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "abc123", std::chrono::seconds(1));

    FakeClock::time += std::chrono::seconds(2);

    assert(!store.ttl("session").has_value());
}

void test_plain_set_clears_previous_expiration() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "abc123", std::chrono::seconds(1));
    store.set("session", "refreshed");

    FakeClock::time += std::chrono::seconds(60);

    // The overwrite removed the deadline, so the key survives.
    assert(store.get("session").value() == "refreshed");
    assert(store.ttl("session").value() == -1);
}

void test_setex_replaces_previous_expiration() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "short", std::chrono::seconds(1));
    store.set_ex("session", "long", std::chrono::seconds(600));

    FakeClock::time += std::chrono::seconds(5);

    // The newer SETEX replaced the original 1-second deadline.
    assert(store.get("session").value() == "long");
}

void test_setex_rejects_duration_that_overflows_clock() {
    KvStore store(&FakeClock::now);

    // Uses the clock's own max duration, so now + duration overflows.
    const bool stored = store.set_ex(
        "session", "x", KvStore::Clock::duration::max()
    );

    assert(!stored);
    assert(!store.get("session").has_value());
}

void test_setex_rejects_ttl_near_max_time_point() {
    // A clock already at time_point::max() leaves no room for any addition.
    KvStore::TimePoint old_time = FakeClock::time;
    FakeClock::time = KvStore::Clock::time_point::max();
    KvStore store(&FakeClock::now);

    // A tiny duration would still overflow when added to max().
    const bool stored =
        store.set_ex("session", "x", std::chrono::seconds(1));

    FakeClock::time = old_time;

    assert(!stored);
    assert(!store.get("session").has_value());
}

void test_setex_rejects_overflow_preserving_existing_deadline() {
    // A clock very close to its maximum leaves almost no room for a new
    // TTL, but the existing key must stay untouched when the new SETEX
    // fails.
    const KvStore::TimePoint near_max =
        KvStore::Clock::time_point::max() - std::chrono::seconds(5);
    KvStore::TimePoint old_time = FakeClock::time;
    FakeClock::time = near_max;
    KvStore store(&FakeClock::now);

    store.set("session", "keep");
    store.set_ex("session", "keep", std::chrono::seconds(2));

    // This would overflow the clock so set_ex rejects it.
    const bool overflow_rejected = store.set_ex(
        "session", "new", std::chrono::seconds(60)
    );

    assert(!overflow_rejected);

    // The clock has not moved, so the 2-second deadline has exactly 2
    // seconds left. Both checks run while the fake clock is still near max.
    assert(store.get("session").value() == "keep");
    assert(store.ttl("session").value() == 2);

    FakeClock::time = old_time;
}

void test_ttl_returns_minus_one_not_zero_near_boundary() {
    // A scripted clock that advances every read catches stale-clock bugs in
    // ttl(). The old two-sample implementation could read the deadline as
    // unexpired and then compute a negative remaining time, misreporting -1.
    // Each store operation consumes exactly one clock reading, so call the
    // store directly rather than through the same convenience accessors.
    static std::vector<KvStore::TimePoint> times;
    static std::size_t times_index = 0;
    const KvStore::TimePoint base = KvStore::Clock::now();
    times = {
        base + std::chrono::milliseconds(0),  // set_ex reads this.
        base + std::chrono::milliseconds(900),  // ttl() reads this.
        base + std::chrono::seconds(2),  // the next call reads this.
    };
    times_index = 0;
    static auto scripted_now = []() -> KvStore::TimePoint {
        return times[times_index++];
    };

    KvStore store(scripted_now);

    // The deadline is 1 second after base. The first ttl() reads 900ms,
    // so the key is still live and exactly zero seconds remain.
    store.set_ex("session", "x", std::chrono::seconds(1));

    const auto first_ttl = store.ttl("session");

    assert(first_ttl.has_value());
    assert(*first_ttl == 0);

    // The next clock reading is 2s, past the 1s deadline, so the key is now
    // missing.
    assert(!store.ttl("session").has_value());
}

void test_delete_expired_key_returns_false() {
    KvStore store(&FakeClock::now);

    store.set_ex("session", "abc123", std::chrono::seconds(1));

    FakeClock::time += std::chrono::seconds(2);

    // An expired key behaves like a missing key for DEL too.
    assert(!store.del("session"));
    assert(!store.get("session").has_value());
}

int main() {
    test_missing_key_returns_empty();
    test_set_then_get_returns_value();
    test_set_overwrites_existing_value();
    test_delete_existing_key_returns_true_and_removes_key();
    test_delete_missing_key_returns_false();
    test_setex_returns_value_before_expiry();
    test_setex_key_disappears_at_expiry();
    test_ttl_reports_whole_seconds_remaining();
    test_ttl_zero_before_one_second_remains();
    test_ttl_is_minus_one_without_expiration();
    test_ttl_is_minus_two_for_missing_key();
    test_ttl_is_minus_two_after_expiry();
    test_plain_set_clears_previous_expiration();
    test_setex_replaces_previous_expiration();
    test_delete_expired_key_returns_false();
    test_setex_rejects_duration_that_overflows_clock();
    test_setex_rejects_ttl_near_max_time_point();
    test_ttl_returns_minus_one_not_zero_near_boundary();
    test_setex_rejects_overflow_preserving_existing_deadline();

    std::cout << "All KvStore tests passed.\n";
    return 0;
}
