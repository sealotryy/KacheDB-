#include <cassert>
#include <iostream>

#include "kv_store.h"

int main() {
    KvStore store;

    // A new store should not contain a key we never set.
    assert(!store.get("name").has_value());

    // SET should make GET return the stored value.
    store.set("name", "Alex");
    auto name = store.get("name");
    assert(name.has_value());
    assert(name.value() == "Alex");

    // SET on the same key should overwrite its old value.
    store.set("name", "Jordan");
    name = store.get("name");
    assert(name.has_value());
    assert(name.value() == "Jordan");

    // DEL should report success and remove the key.
    assert(store.del("name"));
    assert(!store.get("name").has_value());

    // Deleting a missing key should report false.
    assert(!store.del("name"));

    std::cout << "All KvStore tests passed.\n";
    return 0;
}