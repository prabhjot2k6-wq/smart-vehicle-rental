#pragma once
// Customer and their (synthetic) driving licence.
//
// DrivingLicence is a VALUE OBJECT: it is copied, compared and replaced as a
// whole, and it knows its own rule - "does this licence let the holder drive
// category X for the whole rental?"

#include "rental/Date.h"
#include "rental/Enums.h"
#include "rental/Money.h"

#include <memory>
#include <set>
#include <string>

namespace rental {

class DrivingLicence {
public:
    DrivingLicence(std::string number, Date expiry, std::set<VehicleCategory> permitted);

    const std::string& number() const { return number_; }
    Date expiry() const { return expiry_; }
    const std::set<VehicleCategory>& permittedCategories() const { return permitted_; }

    bool permits(VehicleCategory category) const { return permitted_.count(category) > 0; }

    // The customer still has the vehicle on the return date, so the licence
    // must be valid up to AND including period.end().
    bool coversRental(VehicleCategory category, const DateRange& period) const;

    // Same check, but throws ValidationError explaining exactly what is wrong.
    void requireCovers(VehicleCategory category, const DateRange& period) const;

    std::string categoriesText() const; // "Car, Bike"

private:
    std::string number_;
    Date expiry_;
    std::set<VehicleCategory> permitted_;
};

// Customer is an ABSTRACT BASE CLASS. Three concrete kinds exist and each one
// answers the same questions in its own way (POLYMORPHISM):
//   discountBp()         - loyalty discount added to every quote
//   maxActiveBookings()  - how many open bookings the customer may hold
// Code that prices or books never asks "which kind of customer is this?";
// it just calls the virtual functions.
class Customer {
public:
    virtual ~Customer() = default;
    // Copying a base reference would slice the derived part, so it is blocked.
    Customer(const Customer&) = delete;
    Customer& operator=(const Customer&) = delete;

    virtual CustomerType type() const = 0;
    virtual BasisPoints discountBp() const = 0;  // 500 = 5.00%
    virtual int maxActiveBookings() const = 0;   // Confirmed + Active bookings

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& phone() const { return phone_; }
    const std::string& email() const { return email_; }
    const DrivingLicence& licence() const { return licence_; }
    std::string typeName() const { return toString(type()); }

    void updateContact(const std::string& name, const std::string& phone, const std::string& email);
    void replaceLicence(const DrivingLicence& licence) { licence_ = licence; }

protected:
    Customer(std::string id, std::string name, std::string phone, std::string email, DrivingLicence licence);

private:
    std::string id_;
    std::string name_;
    std::string phone_;
    std::string email_;
    DrivingLicence licence_;
};

class RegularCustomer final : public Customer {
public:
    RegularCustomer(std::string id, std::string name, std::string phone, std::string email, DrivingLicence licence)
        : Customer(std::move(id), std::move(name), std::move(phone), std::move(email), std::move(licence)) {}
    CustomerType type() const override { return CustomerType::Regular; }
    BasisPoints discountBp() const override { return 0; }
    int maxActiveBookings() const override { return 5; }
};

class PremiumCustomer final : public Customer {
public:
    PremiumCustomer(std::string id, std::string name, std::string phone, std::string email, DrivingLicence licence)
        : Customer(std::move(id), std::move(name), std::move(phone), std::move(email), std::move(licence)) {}
    CustomerType type() const override { return CustomerType::Premium; }
    BasisPoints discountBp() const override { return 500; }
    int maxActiveBookings() const override { return 8; }
};

class CorporateCustomer final : public Customer {
public:
    CorporateCustomer(std::string id, std::string name, std::string phone, std::string email, DrivingLicence licence)
        : Customer(std::move(id), std::move(name), std::move(phone), std::move(email), std::move(licence)) {}
    CustomerType type() const override { return CustomerType::Corporate; }
    BasisPoints discountBp() const override { return 1000; }
    int maxActiveBookings() const override { return 20; }
};

// FACTORY: builds the right derived class from a CustomerType. Used when a
// customer is registered and when the data file is loaded.
class CustomerFactory {
public:
    static std::unique_ptr<Customer> create(CustomerType type, std::string id, std::string name, std::string phone,
                                            std::string email, DrivingLicence licence);
};

} // namespace rental
