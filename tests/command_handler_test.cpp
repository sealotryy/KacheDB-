#include <cassert>
#include <chrono>
#include <iostream>
#include <string>

#include "command_handler.h"
#include "kv_store.h"

// A controllable clock so TTL tests never depend on real time advancing.
class FakeClock {
public:
    static KvStore::TimePoint time;
    static KvStore::TimePoint now() {
        return time;
    }
};

KvStore::TimePoint FakeClock::time = KvStore::Clock::now();

void test_set_returns_ok_and_saves_value() {
    KvStore store;

    std::string response = execute_command(store, "SET name Alex");

    assert(response == "OK\n");
    assert(store.get("name").has_value());
    assert(store.get("name").value() == "Alex");
}

void test_get_returns_stored_value() {
    KvStore store;
    store.set("name", "Alex");

    std::string response = execute_command(store, "GET name");

    assert(response == "Alex\n");
}

void test_get_missing_key_returns_not_found() {
    KvStore store;

    std::string response = execute_command(store, "GET missing");

    assert(response == "NOT_FOUND\n");
}

void test_set_allows_value_with_spaces() {
    KvStore store;

    std::string response = execute_command(
        store,
        "SET message Hello from my TCP server"
    );

    assert(response == "OK\n");

    std::optional<std::string> value = store.get("message");
    assert(value.has_value());
    assert(value.value() == "Hello from my TCP server");
}

void test_set_without_value_returns_error() {
    KvStore store;

    std::string response = execute_command(store, "SET name");

    assert(response == "ERROR: use SET <key> <value>\n");
}

void test_get_without_key_returns_error() {
    KvStore store;

    std::string response = execute_command(store, "GET");

    assert(response == "ERROR: use GET <key>\n");
}

void test_delete_existing_key_returns_ok_and_removes_value() {
    KvStore store;
    store.set("name", "Alex");

    std::string response = execute_command(store, "DEL name");

    assert(response == "OK\n");
    assert(!store.get("name").has_value());
}

void test_delete_missing_key_returns_not_found() {
    KvStore store;

    std::string response = execute_command(store, "DEL missing");

    assert(response == "NOT_FOUND\n");
}

void test_delete_without_key_returns_error() {
    KvStore store;

    std::string response = execute_command(store, "DEL");

    assert(response == "ERROR: use DEL <key>\n");
}

void test_unknown_command_returns_error() {
    KvStore store;

    std::string response = execute_command(store, "HELLO there");

    assert(response == "ERROR: unknown command\n");
}

void test_empty_command_returns_empty_response() {
    KvStore store;

    std::string response = execute_command(store, "");

    assert(response == "");
}

void test_setex_returns_ok_and_saves_value() {
    KvStore store;

    std::string response = execute_command(store, "SETEX session 60 abc123");

    assert(response == "OK\n");
    assert(store.get("session").value() == "abc123");
}

void test_setex_value_may_contain_spaces() {
    KvStore store;

    std::string response =
        execute_command(store, "SETEX message 60 hello world");

    assert(response == "OK\n");
    assert(store.get("message").value() == "hello world");
}

void test_setex_non_positive_seconds_returns_error() {
    KvStore store;
    store.set("session", "before");

    std::string response = execute_command(store, "SETEX session 0 x");

    assert(response == "ERROR: seconds must be a positive integer\n");
    // A rejected SETEX leaves the existing value untouched.
    assert(store.get("session").value() == "before");
}

void test_setex_negative_seconds_returns_error() {
    KvStore store;
    store.set("session", "before");

    std::string response = execute_command(store, "SETEX session -5 x");

    assert(response == "ERROR: seconds must be a positive integer\n");
    assert(store.get("session").value() == "before");
}

void test_setex_malformed_seconds_returns_error() {
    KvStore store;
    store.set("session", "before");

    std::string response = execute_command(store, "SETEX session 1.5 x");

    assert(response == "ERROR: seconds must be a positive integer\n");
    assert(store.get("session").value() == "before");
}

void test_setex_overflow_seconds_returns_error() {
    KvStore store;
    store.set("session", "before");

    std::string response = execute_command(store, "SETEX session 99999999999999999999 x");

    assert(response == "ERROR: seconds must be a positive integer\n");
    assert(store.get("session").value() == "before");
}

void test_setex_without_value_returns_error() {
    KvStore store;
    store.set("session", "before");

    std::string response = execute_command(store, "SETEX session 60");

    assert(response == "ERROR: use SETEX <key> <seconds> <value>\n");
    assert(store.get("session").value() == "before");
}

void test_setex_without_args_returns_error() {
    KvStore store;

    std::string response = execute_command(store, "SETEX");

    assert(response == "ERROR: use SETEX <key> <seconds> <value>\n");
}

void test_ttl_returns_remaining_seconds() {
    KvStore store(&FakeClock::now);

    execute_command(store, "SETEX session 60 abc123");

    std::string response = execute_command(store, "TTL session");

    assert(response == "60\n");
}

void test_ttl_returns_minus_one_without_expiry() {
    KvStore store;
    store.set("session", "abc123");

    std::string response = execute_command(store, "TTL session");

    assert(response == "-1\n");
}

void test_ttl_returns_minus_two_for_missing_key() {
    KvStore store;

    std::string response = execute_command(store, "TTL missing");

    assert(response == "-2\n");
}

void test_setex_seconds_fitting_long_long_but_not_clock_returns_error() {
    KvStore store(&FakeClock::now);
    store.set_ex("session", "keep", std::chrono::seconds(50));

    // This fits long long but exceeds the clock duration range.
    const std::string response =
        execute_command(store, "SETEX session 9223372036854775807 x");

    assert(response == "ERROR: seconds is too large\n");
    // The existing value and its expiration are preserved.
    assert(store.get("session").value() == "keep");
    assert(store.ttl("session").value() == 50);
}

void test_ttl_without_key_returns_error() {
    KvStore store;

    std::string response = execute_command(store, "TTL");

    assert(response == "ERROR: use TTL <key>\n");
}

int main() {
    test_set_returns_ok_and_saves_value();
    test_get_returns_stored_value();
    test_get_missing_key_returns_not_found();
    test_set_allows_value_with_spaces();
    test_set_without_value_returns_error();
    test_get_without_key_returns_error();
    test_delete_existing_key_returns_ok_and_removes_value();
    test_delete_missing_key_returns_not_found();
    test_delete_without_key_returns_error();
    test_unknown_command_returns_error();
    test_empty_command_returns_empty_response();
    test_setex_returns_ok_and_saves_value();
    test_setex_value_may_contain_spaces();
    test_setex_non_positive_seconds_returns_error();
    test_setex_negative_seconds_returns_error();
    test_setex_malformed_seconds_returns_error();
    test_setex_overflow_seconds_returns_error();
    test_setex_without_value_returns_error();
    test_setex_without_args_returns_error();
    test_ttl_returns_remaining_seconds();
    test_ttl_returns_minus_one_without_expiry();
    test_ttl_returns_minus_two_for_missing_key();
    test_setex_seconds_fitting_long_long_but_not_clock_returns_error();
    test_ttl_without_key_returns_error();

    std::cout << "All command-handler tests passed.\n";
    return 0;
}