#pragma once

#include <cstddef>
#include <string>

// Parses sizes like "32KB", "1MB", "4096", "64" (case-insensitive
// K/KB/M/MB suffix, or plain bytes with no suffix). Throws
// std::invalid_argument on malformed input.
size_t parse_size(const std::string& text);
