#include "kv_store.h"

void KvStore::set(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::optional<std::string> KvStore::get(const std::string& key) const {
    auto it = data_.find(key);

    if (it == data_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool KvStore::del(const std::string& key) {
    return data_.erase(key) > 0;
}