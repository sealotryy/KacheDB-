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
        
        // SET 
        if (command == "SET") {
            std::string key;
            
            // input >> key reads the key
            if (!(input >> key)) {
                std::cout << "ERROR: use SET <key> <value>\n";
                continue;
            }

            // input >> std::ws discards leading whitespace from input stream
            std::string value;
            // reads all remaining text into value, so we can work w longer strings instead of single words
            std::getline(input >> std::ws, value);
            if (value.empty()) {
                std::cout << "ERROR: use SET <key> <value>\n";
                continue;
            }

            store.set(key, value);
            std::cout << "OK\n";
        } 
            
        // GET
        else if (command == "GET") {
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
        }
        
        // DEL 
        else if (command == "DEL") {
            std::string key;

            if (!(input >> key)) {
                std::cout << "ERROR: use DEL <key>\n";
                continue;
            }

            bool deleted = store.del(key);

            if (deleted) {
                std::cout << "OK\n";
            } else {
                std::cout << "NOT_FOUND\n";
            }
        } 
            
        else if (command == "EXIT") {
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