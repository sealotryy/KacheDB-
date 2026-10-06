#include "command_handler.h"

#include <sstream>
#include <string>

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