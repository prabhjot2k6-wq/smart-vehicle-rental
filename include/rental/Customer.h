#pragma once
// Customer and their (synthetic) driving licence.
//
// DrivingLicence is a VALUE OBJECT: it is copied, compared and replaced as a
// whole, and it knows its own rule - "does this licence let the holder drive
// category X for the whole rental?"

#include "rental/Date.h"
#include "rental/Enums.h"

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

class Customer {
public:
    Customer(std::string id, std::string name, std::string phone, std::string email, DrivingLicence licence);

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& phone() const { return phone_; }
    const std::string& email() const { return email_; }
    const DrivingLicence& licence() const { return licence_; }

    void updateContact(const std::string& name, const std::string& phone, const std::string& email);
    void replaceLicence(const DrivingLicence& licence) { licence_ = licence; }

private:
    std::string id_;
    std::string name_;
    std::string phone_;
    std::string email_;
    DrivingLicence licence_;
};

} // namespace rental
