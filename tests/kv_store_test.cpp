#include <cassert>
#include <iostream>
#include <string>

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

int main() {
    test_missing_key_returns_empty();
    test_set_then_get_returns_value();
    test_set_overwrites_existing_value();
    test_delete_existing_key_returns_true_and_removes_key();
    test_delete_missing_key_returns_false();

    std::cout << "All KvStore tests passed.\n";
    return 0;
}