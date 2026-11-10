#pragma once
// Console input/output helpers.
//
// Every prompt re-asks until the input is valid, so bad numbers or dates can
// never crash the program or leave std::cin in a failed state (we always read
// whole lines with std::getline and parse them ourselves).
//
// Typing "cancel" at any prompt throws InputCancelled, which takes the user
// back to the current menu. End-of-input (Ctrl+D / Ctrl+Z) throws InputClosed.

#include "rental/Date.h"
#include "rental/Money.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace rental {

struct InputCancelled {};
struct InputClosed {};

class ConsoleIO {
public:
    ConsoleIO(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

    std::ostream& out() { return out_; }

    std::string askText(const std::string& prompt);                              // non-empty
    std::string askOptionalText(const std::string& prompt);                      // may be empty
    std::string askTextDefault(const std::string& prompt, const std::string& current); // blank keeps current
    long long askInt(const std::string& prompt, long long min, long long max);
    long long askIntDefault(const std::string& prompt, long long min, long long max, long long current);
    Money askMoney(const std::string& prompt);
    Money askMoneyDefault(const std::string& prompt, Money current);
    Date askDate(const std::string& prompt);
    Date askDateDefault(const std::string& prompt, Date current);
    bool confirm(const std::string& question); // y / n

    // Prints a numbered menu with "0. Back" and returns the chosen number.
    int menu(const std::string& title, const std::vector<std::string>& options, const std::string& zeroLabel = "Back");
    // Picks one item from a list of labels; returns the zero-based index.
    std::size_t choose(const std::string& prompt, const std::vector<std::string>& labels);

    void heading(const std::string& title);
    void info(const std::string& message);
    void error(const std::string& message);

private:
    std::string readLine(const std::string& prompt);
    std::istream& in_;
    std::ostream& out_;
};

// Simple aligned text table using ASCII only (portable to every terminal).
class TextTable {
public:
    explicit TextTable(std::vector<std::string> headers) : headers_(std::move(headers)) {}
    void addRow(std::vector<std::string> cells) { rows_.push_back(std::move(cells)); }
    void alignRight(std::size_t column) { rightAligned_.push_back(column); }
    bool empty() const { return rows_.empty(); }
    void print(std::ostream& os) const;

private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<std::size_t> rightAligned_;
};

} // namespace rental
