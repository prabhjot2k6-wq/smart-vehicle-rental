#include "rental/TextUtil.h"

#include "rental/Errors.h"

#include <algorithm>
#include <cctype>

namespace rental {

std::string trim(const std::string& s) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    auto begin = std::find_if_not(s.begin(), s.end(), isSpace);
    auto end = std::find_if_not(s.rbegin(), s.rend(), isSpace).base();
    return begin < end ? std::string(begin, end) : std::string();
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string requireText(const std::string& value, const std::string& field) {
    std::string t = trim(value);
    if (t.empty()) throw ValidationError(field + " must not be empty.");
    if (t.size() > 120) throw ValidationError(field + " is too long (max 120 characters).");
    return t;
}

} // namespace rental
