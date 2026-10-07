#include <cassert>
#include <iostream>
#include <string>

#include "command_handler.h"
#include "kv_store.h"

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

    std::cout << "All command-handler tests passed.\n";
    return 0;
}