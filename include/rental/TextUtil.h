#pragma once
// Small string helpers shared by domain validation.

#include <string>

namespace rental {

std::string trim(const std::string& s);
std::string toUpper(std::string s);
// Trims and throws ValidationError("<field> must not be empty") if nothing is left.
std::string requireText(const std::string& value, const std::string& field);

} // namespace rental
