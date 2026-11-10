#include "rental/Customer.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <cctype>

namespace rental {

namespace {

std::string validatePhone(const std::string& raw) {
    const std::string phone = requireText(raw, "Phone");
    int digits = 0;
    for (std::size_t i = 0; i < phone.size(); ++i) {
        const char c = phone[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            ++digits;
        } else if (!(c == ' ' || c == '-' || (c == '+' && i == 0))) {
            throw ValidationError("Phone may contain digits, spaces, '-' and a leading '+' only.");
        }
    }
    if (digits < 7 || digits > 15) throw ValidationError("Phone must contain 7-15 digits.");
    return phone;
}

std::string validateEmail(const std::string& raw) {
    const std::string email = requireText(raw, "Email");
    const auto at = email.find('@');
    if (at == std::string::npos || at == 0 || email.find('@', at + 1) != std::string::npos ||
        email.find('.', at) == std::string::npos || email.back() == '.' || email.find(' ') != std::string::npos) {
        throw ValidationError("Email '" + email + "' does not look valid (expected name@domain.tld).");
    }
    return email;
}

} // namespace

DrivingLicence::DrivingLicence(std::string number, Date expiry, std::set<VehicleCategory> permitted)
    : number_(toUpper(requireText(number, "Licence number"))), expiry_(expiry), permitted_(std::move(permitted)) {
    if (permitted_.empty()) throw ValidationError("A licence must permit at least one vehicle category.");
}

bool DrivingLicence::coversRental(VehicleCategory category, const DateRange& period) const {
    return permits(category) && period.end() <= expiry_;
}

void DrivingLicence::requireCovers(VehicleCategory category, const DateRange& period) const {
    if (!permits(category)) {
        throw ValidationError("Licence " + number_ + " does not permit category " + toString(category) +
                              " (permitted: " + categoriesText() + ").");
    }
    if (expiry_ < period.end()) {
        throw ValidationError("Licence " + number_ + " expires on " + expiry_.toString() +
                              ", before the vehicle is due back on " + period.end().toString() + ".");
    }
}

std::string DrivingLicence::categoriesText() const {
    std::string s;
    for (VehicleCategory c : permitted_) s += (s.empty() ? "" : ", ") + toString(c);
    return s;
}

Customer::Customer(std::string id, std::string name, std::string phone, std::string email, DrivingLicence licence)
    : id_(requireText(id, "Customer ID")), licence_(std::move(licence)) {
    updateContact(name, phone, email);
}

void Customer::updateContact(const std::string& name, const std::string& phone, const std::string& email) {
    // Validate everything first so a bad email doesn't leave a half-updated customer.
    std::string n = requireText(name, "Name");
    std::string p = validatePhone(phone);
    std::string e = validateEmail(email);
    name_ = std::move(n);
    phone_ = std::move(p);
    email_ = std::move(e);
}

} // namespace rental
