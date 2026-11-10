#pragma once
// Shared test fixture: a small fleet with a fixed, controllable clock.
// "Today" is Monday 2026-11-09 at 09:00.

#include "rental/Errors.h"
#include "rental/RentalSystem.h"

#include <ostream>
#include <string>
#include <vector>

// Lets CHECK_EQ print enum values in failure messages (found by ADL).
namespace rental {
inline std::ostream& operator<<(std::ostream& os, BookingStatus s) { return os << toString(s); }
inline std::ostream& operator<<(std::ostream& os, VehicleStatus s) { return os << toString(s); }
inline std::ostream& operator<<(std::ostream& os, InvoiceStatus s) { return os << toString(s); }
} // namespace rental

namespace testhelp {

using namespace rental;

inline Date D(const char* text) { return Date::parse(text); }
inline DateRange R(const char* start, const char* end) { return DateRange(D(start), D(end)); }

struct Fleet {
    ManualClock clock{D("2026-11-09")};
    RentalSystem sys{RentalData{}, clock};
    std::string swift, creta, innova;    // model IDs
    std::vector<std::string> swifts;     // 4 vehicles
    std::vector<std::string> cretas;     // 2 vehicles
    std::vector<std::string> innovas;    // 2 vehicles
    std::string customer;                // licence for every category until 2035

    Fleet() {
        swift = sys.fleet().addModel("Maruti", "Swift", VehicleCategory::Car, Money::fromRupees(1500)).id();
        creta = sys.fleet().addModel("Hyundai", "Creta", VehicleCategory::Car, Money::fromRupees(2500)).id();
        innova = sys.fleet().addModel("Toyota", "Innova", VehicleCategory::Car, Money::fromRupees(3500)).id();
        for (int i = 1; i <= 4; ++i) swifts.push_back(addCar(swift, "MH12SW000" + std::to_string(i)));
        for (int i = 1; i <= 2; ++i) cretas.push_back(addCar(creta, "MH12CR000" + std::to_string(i)));
        for (int i = 1; i <= 2; ++i) innovas.push_back(addCar(innova, "MH12IN000" + std::to_string(i)));
        customer = addCustomer("TEST-LIC-1", D("2035-01-01"),
                               {VehicleCategory::Car, VehicleCategory::Bike, VehicleCategory::Van});
    }

    std::string addCar(const std::string& modelId, const std::string& reg) {
        return sys.fleet().addVehicle(modelId, reg, FuelType::Petrol, 10000, 5).id();
    }

    std::string addCustomer(const std::string& licence, Date expiry, std::set<VehicleCategory> cats) {
        return sys.customers()
            .registerCustomer("Test Customer " + licence, "+91 90000 00000", "test@example.com",
                              DrivingLicence(licence, expiry, std::move(cats)))
            .id();
    }

    // Quote + confirm in one step.
    const Booking& book(const std::string& vehicleId, const DateRange& period) {
        const std::string quoteId = sys.quotes().createQuote(customer, vehicleId, period).id;
        return sys.rentals().confirmQuote(quoteId);
    }
};

} // namespace testhelp
