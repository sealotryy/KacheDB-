#include <iostream>
#include <string>

#include "command_handler.h"
#include "kv_store.h"

int main() {
    // This one store stays alive for the entire terminal session.
    KvStore store;

    // Holds one line that the user types.
    std::string line;

    std::cout << "Simple KV store\n";
    std::cout << "Commands: SET <key> <value>, GET <key>, DEL <key>, EXIT\n";

    while (true) {
        std::cout << "> ";

        // Read one full command line, such as: SET name Alex
        // Stop cleanly if the user presses Ctrl+D / input ends.
        if (!std::getline(std::cin, line)) {
            break;
        }

        // EXIT is handled by the terminal program itself because it controls
        // whether this interactive program keeps running.
        if (line == "EXIT") {
            std::cout << "Goodbye\n";
            break;
        }

        // Ask the shared command handler to parse and execute SET/GET/DEL.
        // It returns the exact response text to show the user.
        std::cout << execute_command(store, line);
    }

    return 0;
}