#include "parse_size.hpp"

#include <cctype>
#include <stdexcept>

size_t parse_size(const std::string& text) {
    size_t i = 0;
    while (i < text.size() && std::isdigit((unsigned char)text[i])) i++;
    if (i == 0) throw std::invalid_argument("expected a number at the start of '" + text + "'");

    unsigned long long value = std::stoull(text.substr(0, i));
    std::string suffix = text.substr(i);
    for (char& c : suffix) c = (char)std::toupper((unsigned char)c);

    unsigned long long multiplier;
    if (suffix.empty() || suffix == "B") multiplier = 1;
    else if (suffix == "K" || suffix == "KB") multiplier = 1024ULL;
    else if (suffix == "M" || suffix == "MB") multiplier = 1024ULL * 1024;
    else if (suffix == "G" || suffix == "GB") multiplier = 1024ULL * 1024 * 1024;
    else throw std::invalid_argument("unknown size suffix '" + suffix + "' in '" + text + "'");

    return (size_t)(value * multiplier);
}
