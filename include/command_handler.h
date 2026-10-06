#pragma once

#include <string>

#include "kv_store.h"

// Takes one complete command line, changes the store if needed,
// and returns the text response that should go back to the caller/client.
std::string execute_command(KvStore& store, const std::string& line);