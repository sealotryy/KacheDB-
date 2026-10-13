#include "command_handler.h"

#include <charconv>
#include <chrono>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

namespace {

// Parses a non-negative integer with a radix-prefixed parse. Accepts only
// strings that are entirely digits, rejecting signs, blanks, and overflow.
std::optional<long long> parse_positive_seconds(const std::string& token) {
    if (token.empty() || token.front() == '-' || token.front() == '+') {
        return std::nullopt;
    }

    long long seconds = 0;
    const auto [end, ec] = std::from_chars(
        token.data(), token.data() + token.size(), seconds
    );

    if (ec != std::errc() || end != token.data() + token.size()) {
        return std::nullopt;
    }

    if (seconds <= 0) {
        return std::nullopt;
    }

    return seconds;
}

}  // namespace

std::string execute_command(KvStore& store, const std::string& line) {
    // Treat the line of text as an input source we can read word by word.
    std::istringstream input(line);

    // Read the first word: SET, GET, DEL, etc.
    std::string command;
    input >> command;

    if (command == "SET") {
        std::string key;

        // SET requires at least a key.
        if (!(input >> key)) {
            return "ERROR: use SET <key> <value>\n";
        }

        // Skip the separating whitespace, then take all remaining text
        // as the value so values can contain spaces.
        std::string value;
        std::getline(input >> std::ws, value);

        if (value.empty()) {
            return "ERROR: use SET <key> <value>\n";
        }

        store.set(key, value);
        return "OK\n";
    }

    if (command == "SETEX") {
        std::string key;
        std::string seconds_text;

        if (!(input >> key) || !(input >> seconds_text)) {
            return "ERROR: use SETEX <key> <seconds> <value>\n";
        }

        const auto seconds = parse_positive_seconds(seconds_text);

        if (!seconds.has_value()) {
            return "ERROR: seconds must be a positive integer\n";
        }

        // Skip the separating whitespace, then take all remaining text
        // as the value so values can contain spaces.
        std::string value;
        std::getline(input >> std::ws, value);

        if (value.empty()) {
            return "ERROR: use SETEX <key> <seconds> <value>\n";
        }

        // Cap the parsed seconds before converting to a duration so the
        // conversion itself cannot overflow. Casting the clock's maximum
        // duration to seconds is safe; the reverse (multiplying max seconds
        // by the tick ratio) would overflow first.
        const auto max_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::duration::max()
            ).count();

        if (*seconds > max_seconds) {
            return "ERROR: seconds is too large\n";
        }

        // A rejected SETEX must not overwrite an existing key's value or
        // expiration, so a failure at this step leaves the key untouched.
        if (!store.set_ex(key, value, std::chrono::seconds(*seconds))) {
            return "ERROR: seconds is too large\n";
        }

        return "OK\n";
    }

    if (command == "TTL") {
        std::string key;

        if (!(input >> key)) {
            return "ERROR: use TTL <key>\n";
        }

        auto remaining = store.ttl(key);

        if (!remaining.has_value()) {
            return "-2\n";
        }

        if (*remaining < 0) {
            return "-1\n";
        }

        return std::to_string(*remaining) + "\n";
    }

    if (command == "GET") {
        std::string key;

        if (!(input >> key)) {
            return "ERROR: use GET <key>\n";
        }

        auto value = store.get(key);

        if (value.has_value()) {
            return value.value() + "\n";
        }

        return "NOT_FOUND\n";
    }

    if (command == "DEL") {
        std::string key;

        if (!(input >> key)) {
            return "ERROR: use DEL <key>\n";
        }

        if (store.del(key)) {
            return "OK\n";
        }

        return "NOT_FOUND\n";
    }

    if (command.empty()) {
        return "";
    }

    return "ERROR: unknown command\n";
}