#pragma once
// CustomerService: registering and updating customers.
// Enforces that no two customers share a licence number.

#include "rental/RentalData.h"

#include <string>
#include <vector>

namespace rental {

class CustomerService {
public:
    explicit CustomerService(RentalData& data) : data_(data) {}

    // Registers a Regular customer (the default kind).
    const Customer& registerCustomer(const std::string& name, const std::string& phone, const std::string& email,
                                     const DrivingLicence& licence);
    // Same, but for a chosen kind: Regular, Premium or Corporate.
    const Customer& registerCustomer(CustomerType type, const std::string& name, const std::string& phone,
                                     const std::string& email, const DrivingLicence& licence);
    void removeCustomer(const std::string& customerId);
    void updateDetails(const std::string& customerId, const std::string& name, const std::string& phone,
                       const std::string& email, const DrivingLicence& licence);
    void updateContact(const std::string& customerId, const std::string& name, const std::string& phone,
                       const std::string& email);
    void updateLicence(const std::string& customerId, const DrivingLicence& licence);

    const Customer& customer(const std::string& id) const { return data_.customers.get(id); }
    std::vector<const Customer*> customers() const { return data_.customers.all(); }

private:
    void requireUniqueLicence(const std::string& licenceNumber, const std::string& exceptCustomerId) const;
    RentalData& data_;
};

} // namespace rental


