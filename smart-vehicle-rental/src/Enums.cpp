#include "rental/Enums.h"

#include "rental/Errors.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace rental {

namespace {

template <typename E>
using NameTable = std::vector<std::pair<E, const char*>>;

const NameTable<VehicleCategory>& categoryNames() {
    static const NameTable<VehicleCategory> t = {
        {VehicleCategory::Car, "Car"}, {VehicleCategory::Bike, "Bike"}, {VehicleCategory::Van, "Van"}};
    return t;
}
const NameTable<FuelType>& fuelNames() {
    static const NameTable<FuelType> t = {{FuelType::Petrol, "Petrol"},
                                          {FuelType::Diesel, "Diesel"},
                                          {FuelType::CNG, "CNG"},
                                          {FuelType::Electric, "Electric"},
                                          {FuelType::Hybrid, "Hybrid"}};
    return t;
}
const NameTable<VehicleStatus>& vehicleStatusNames() {
    static const NameTable<VehicleStatus> t = {{VehicleStatus::Active, "Active"},
                                               {VehicleStatus::UnderMaintenance, "UnderMaintenance"},
                                               {VehicleStatus::Inactive, "Inactive"}};
    return t;
}
const NameTable<BookingStatus>& bookingStatusNames() {
    static const NameTable<BookingStatus> t = {{BookingStatus::Confirmed, "Confirmed"},
                                               {BookingStatus::Active, "Active"},
                                               {BookingStatus::Completed, "Completed"},
                                               {BookingStatus::Cancelled, "Cancelled"}};
    return t;
}
const NameTable<MaintenanceStatus>& maintenanceStatusNames() {
    static const NameTable<MaintenanceStatus> t = {{MaintenanceStatus::Open, "Open"},
                                                   {MaintenanceStatus::Completed, "Completed"}};
    return t;
}
const NameTable<InvoiceStatus>& invoiceStatusNames() {
    static const NameTable<InvoiceStatus> t = {{InvoiceStatus::Draft, "Draft"},
                                               {InvoiceStatus::Finalized, "Finalized"}};
    return t;
}
const NameTable<CustomerType>& customerTypeNames() {
    static const NameTable<CustomerType> t = {{CustomerType::Regular, "Regular"},
                                              {CustomerType::Premium, "Premium"},
                                              {CustomerType::Corporate, "Corporate"}};
    return t;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

template <typename E>
std::string nameOf(const NameTable<E>& table, E value) {
    for (const auto& entry : table) {
        if (entry.first == value) return entry.second;
    }
    return "?";
}

template <typename E>
E parseName(const NameTable<E>& table, const std::string& text, const char* what) {
    const std::string wanted = lower(text);
    std::string options;
    for (const auto& entry : table) {
        if (lower(entry.second) == wanted) return entry.first;
        options += (options.empty() ? "" : ", ") + std::string(entry.second);
    }
    throw ValidationError(std::string("Unknown ") + what + " '" + text + "'. Expected one of: " + options);
}

} // namespace

std::string toString(VehicleCategory v) { return nameOf(categoryNames(), v); }
std::string toString(FuelType v) { return nameOf(fuelNames(), v); }
std::string toString(VehicleStatus v) { return nameOf(vehicleStatusNames(), v); }
std::string toString(BookingStatus v) { return nameOf(bookingStatusNames(), v); }
std::string toString(MaintenanceStatus v) { return nameOf(maintenanceStatusNames(), v); }
std::string toString(InvoiceStatus v) { return nameOf(invoiceStatusNames(), v); }
std::string toString(CustomerType v) { return nameOf(customerTypeNames(), v); }

VehicleCategory parseVehicleCategory(const std::string& t) { return parseName(categoryNames(), t, "vehicle category"); }
FuelType parseFuelType(const std::string& t) { return parseName(fuelNames(), t, "fuel type"); }
VehicleStatus parseVehicleStatus(const std::string& t) { return parseName(vehicleStatusNames(), t, "vehicle status"); }
BookingStatus parseBookingStatus(const std::string& t) { return parseName(bookingStatusNames(), t, "booking status"); }
MaintenanceStatus parseMaintenanceStatus(const std::string& t) {
    return parseName(maintenanceStatusNames(), t, "maintenance status");
}
InvoiceStatus parseInvoiceStatus(const std::string& t) { return parseName(invoiceStatusNames(), t, "invoice status"); }
CustomerType parseCustomerType(const std::string& t) { return parseName(customerTypeNames(), t, "customer type"); }

const std::vector<VehicleCategory>& allVehicleCategories() {
    static const std::vector<VehicleCategory> v = {VehicleCategory::Car, VehicleCategory::Bike, VehicleCategory::Van};
    return v;
}

const std::vector<FuelType>& allFuelTypes() {
    static const std::vector<FuelType> v = {FuelType::Petrol, FuelType::Diesel, FuelType::CNG, FuelType::Electric,
                                            FuelType::Hybrid};
    return v;
}

const std::vector<CustomerType>& allCustomerTypes() {
    static const std::vector<CustomerType> v = {CustomerType::Regular, CustomerType::Premium, CustomerType::Corporate};
    return v;
}

} // namespace rental
