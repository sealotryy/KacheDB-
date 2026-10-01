#include <iostream>
#include <sstream>
#include <string>

#include "kv_store.h"

int main() {
    KvStore store;
    std::string line;

    while (true) {
        std::cout << "> ";

        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream input(line);

        std::string command;
        input >> command;

        if (command == "SET") {
            std::string key;
            std::string value;

            if (!(input >> key >> value)) {
                std::cout << "ERROR: use SET <key> <value>\n";
                continue;
            }

            store.set(key, value);
            std::cout << "OK\n";
        } else if (command == "GET") {
            std::string key;

            if (!(input >> key)) {
                std::cout << "ERROR: use GET <key>\n";
                continue;
            }

            auto value = store.get(key);

            if (value.has_value()) {
                std::cout << value.value() << "\n";
            } else {
                std::cout << "NOT_FOUND\n";
            }
        } else if (command == "EXIT") {
            std::cout << "Goodbye\n";
            break;
        } else if (command.empty()) {
            continue;
        } else {
            std::cout << "ERROR: unknown command\n";
        }
    }

    return 0;
}