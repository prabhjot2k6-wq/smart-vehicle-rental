#pragma once
// Strongly typed enumerations (enum class) plus text conversion.
// toString() gives a stable token that is also used in the data file;
// parseXxx() is case-insensitive and throws ValidationError on unknown text.

#include <string>
#include <vector>

namespace rental {

enum class VehicleCategory { Car, Bike, Van };
enum class FuelType { Petrol, Diesel, CNG, Electric, Hybrid };

// Operational status of a vehicle. "Rented out" is deliberately NOT a status:
// it is derived from bookings, so there is a single source of truth.
enum class VehicleStatus { Active, UnderMaintenance, Inactive };

// Confirmed = reserved, not yet collected; Active = vehicle is with the customer.
enum class BookingStatus { Confirmed, Active, Completed, Cancelled };

enum class MaintenanceStatus { Open, Completed };
enum class InvoiceStatus { Draft, Finalized };

// Kind of customer. Each kind is a different class (see Customer.h).
enum class CustomerType { Regular, Premium, Corporate };

std::string toString(VehicleCategory v);
std::string toString(FuelType v);
std::string toString(VehicleStatus v);
std::string toString(BookingStatus v);
std::string toString(MaintenanceStatus v);
std::string toString(InvoiceStatus v);
std::string toString(CustomerType v);

VehicleCategory parseVehicleCategory(const std::string& text);
FuelType parseFuelType(const std::string& text);
VehicleStatus parseVehicleStatus(const std::string& text);
BookingStatus parseBookingStatus(const std::string& text);
MaintenanceStatus parseMaintenanceStatus(const std::string& text);
InvoiceStatus parseInvoiceStatus(const std::string& text);
CustomerType parseCustomerType(const std::string& text);

const std::vector<VehicleCategory>& allVehicleCategories();
const std::vector<FuelType>& allFuelTypes();
const std::vector<CustomerType>& allCustomerTypes();

} // namespace rental
