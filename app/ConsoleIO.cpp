#include "ConsoleIO.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>

namespace rental {

std::string ConsoleIO::readLine(const std::string& prompt) {
    out_ << prompt << ": " << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        out_ << "\n";
        throw InputClosed{};
    }
    if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows line endings in piped input
    line = trim(line);
    std::string lowered = line;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "cancel") throw InputCancelled{};
    return line;
}

std::string ConsoleIO::askText(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt);
        if (!s.empty()) return s;
        error("A value is required (type 'cancel' to go back).");
    }
}

std::string ConsoleIO::askTextDefault(const std::string& prompt, const std::string& current) {
    std::string s = readLine(prompt + " [" + current + "]");
    return s.empty() ? current : s;
}

namespace {
bool parseInteger(const std::string& s, long long& value) {
    if (s.empty()) return false;
    std::size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i == s.size()) return false;
    for (std::size_t j = i; j < s.size(); ++j) {
        if (!std::isdigit(static_cast<unsigned char>(s[j]))) return false;
    }
    if (s.size() > 18) return false;
    value = std::stoll(s);
    return true;
}
} // namespace

long long ConsoleIO::askInt(const std::string& prompt, long long min, long long max) {
    while (true) {
        long long v = 0;
        const std::string s = readLine(prompt);
        if (parseInteger(s, v) && v >= min && v <= max) return v;
        error("Please enter a whole number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    }
}

long long ConsoleIO::askIntDefault(const std::string& prompt, long long min, long long max, long long current) {
    while (true) {
        long long v = 0;
        const std::string s = readLine(prompt + " [" + std::to_string(current) + "]");
        if (s.empty()) return current;
        if (parseInteger(s, v) && v >= min && v <= max) return v;
        error("Please enter a whole number from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    }
}

Money ConsoleIO::askMoney(const std::string& prompt) {
    while (true) {
        try {
            return Money::parse(readLine(prompt + " (INR)"));
        } catch (const ValidationError& e) {
            error(e.what());
        }
    }
}

Money ConsoleIO::askMoneyDefault(const std::string& prompt, Money current) {
    while (true) {
        const std::string s = readLine(prompt + " (INR) [" + current.toString() + "]");
        if (s.empty()) return current;
        try {
            return Money::parse(s);
        } catch (const ValidationError& e) {
            error(e.what());
        }
    }
}

Date ConsoleIO::askDate(const std::string& prompt) {
    while (true) {
        try {
            return Date::parse(readLine(prompt + " (YYYY-MM-DD)"));
        } catch (const ValidationError& e) {
            error(e.what());
        }
    }
}

Date ConsoleIO::askDateDefault(const std::string& prompt, Date current) {
    while (true) {
        const std::string s = readLine(prompt + " (YYYY-MM-DD) [" + current.toString() + "]");
        if (s.empty()) return current;
        try {
            return Date::parse(s);
        } catch (const ValidationError& e) {
            error(e.what());
        }
    }
}

bool ConsoleIO::confirm(const std::string& question) {
    while (true) {
        const std::string s = readLine(question + " (y/n)");
        if (s == "y" || s == "Y" || s == "yes") return true;
        if (s == "n" || s == "N" || s == "no") return false;
        error("Please answer y or n.");
    }
}

int ConsoleIO::menu(const std::string& title, const std::vector<std::string>& options, const std::string& zeroLabel) {
    heading(title);
    for (std::size_t i = 0; i < options.size(); ++i) out_ << "  " << (i + 1) << ". " << options[i] << "\n";
    out_ << "  0. " << zeroLabel << "\n";
    while (true) {
        long long v = 0;
        std::string s;
        try {
            s = readLine("Choose");
        } catch (const InputCancelled&) {
            return 0;
        }
        if (parseInteger(s, v) && v >= 0 && v <= static_cast<long long>(options.size())) return static_cast<int>(v);
        error("Please choose a number from the menu.");
    }
}

std::size_t ConsoleIO::choose(const std::string& prompt, const std::vector<std::string>& labels) {
    for (std::size_t i = 0; i < labels.size(); ++i) out_ << "  " << (i + 1) << ". " << labels[i] << "\n";
    return static_cast<std::size_t>(askInt(prompt, 1, static_cast<long long>(labels.size())) - 1);
}

void ConsoleIO::heading(const std::string& title) {
    out_ << "\n=== " << title << " " << std::string(title.size() < 50 ? 50 - title.size() : 0, '=') << "\n";
}

void ConsoleIO::info(const std::string& message) { out_ << message << "\n"; }

void ConsoleIO::error(const std::string& message) { out_ << "  ! " << message << "\n"; }

void TextTable::print(std::ostream& os) const {
    std::vector<std::size_t> widths(headers_.size(), 0);
    for (std::size_t c = 0; c < headers_.size(); ++c) widths[c] = headers_[c].size();
    for (const auto& row : rows_) {
        for (std::size_t c = 0; c < row.size() && c < widths.size(); ++c) widths[c] = std::max(widths[c], row[c].size());
    }
    const auto separator = [&]() {
        os << "+";
        for (std::size_t w : widths) os << std::string(w + 2, '-') << "+";
        os << "\n";
    };
    const auto printRow = [&](const std::vector<std::string>& row) {
        os << "|";
        for (std::size_t c = 0; c < widths.size(); ++c) {
            const std::string cell = c < row.size() ? row[c] : "";
            const std::string padding(widths[c] - cell.size(), ' ');
            const bool right = std::find(rightAligned_.begin(), rightAligned_.end(), c) != rightAligned_.end();
            os << " " << (right ? padding + cell : cell + padding) << " |";
        }
        os << "\n";
    };
    separator();
    printRow(headers_);
    separator();
    for (const auto& row : rows_) printRow(row);
    separator();
    if (rows_.empty()) os << "  (no records)\n";
}

} // namespace rental
