#pragma once
// ConsoleApp: menus only. It reads input, calls a service, prints the result.
// It never decides business rules (availability, pricing, charges...): if a
// rule is broken, the service throws and the menu prints the message.

#include "ConsoleIO.h"
#include "rental/DataStore.h"
#include "rental/RentalSystem.h"

#include <functional>
#include <set>
#include <string>

namespace rental {

class ConsoleApp {
public:
    // `store` may be null (nothing is saved), e.g. for experiments.
    ConsoleApp(RentalSystem& system, ConsoleIO& io, const DataStore* store) : sys_(system), io_(io), store_(store) {}
    void run();

private:
    // Runs one menu action: catches rule violations / cancellation and
    // reports them, so a mistake never ends the program.
    void runAction(const std::function<void()>& action);
    void dataChanged(); // saves after every successful change
    bool saveNow();

    // ---- 1. Vehicles (ConsoleApp.cpp) ----
    void vehicleMenu();
    void listModels();
    void addModel();
    void editModel();
    void listVehicles();
    void addVehicle();
    void updateVehicle();
    void deactivateVehicle();
    void reactivateVehicle();

    // ---- 2. Customers (ConsoleApp.cpp) ----
    void customerMenu();
    void listCustomers();
    void registerCustomer();
    void updateCustomer();
    DrivingLicence askLicence(const DrivingLicence* current);

    // ---- 3. Search & quotes, 4. Bookings (ConsoleBooking.cpp) ----
    void quoteMenu();
    void searchAvailability();
    void createQuote();
    void confirmPendingQuote();
    void listPendingQuotes();
    void pricingSettings();
    void bookingMenu();
    void listBookings();
    void viewBooking();
    void extendBooking();
    void cancelBooking();
    void printBreakdown(const PriceBreakdown& breakdown);
    void printBookingSummary(const Booking& booking);
    std::string askBookingId();

    // ---- 5. Pickup & return, 6. Maintenance (ConsoleOperations.cpp) ----
    void operationsMenu();
    void collectVehicle();
    void returnVehicle();
    void viewInvoice();
    void listInvoices();
    void printInvoice(const Invoice& invoice);
    InspectionReport askInspection(const Vehicle& vehicle, long long minimumOdometer, int defaultFuel);
    void maintenanceMenu();
    void listMaintenance();
    void openMaintenance();
    void completeMaintenance();
    void serviceWarnings();

    // ---- 7. Reports (ConsoleReports.cpp) ----
    void reportsMenu();
    void fleetReport();
    void activeRentalsReport();
    void maintenanceDueReport();
    void revenueReport();

    // ---- shared pickers ----
    std::string pickModelId();
    std::string askVehicleId();
    std::string askCustomerId();
    VehicleCategory askCategory();
    FuelType askFuelType(FuelType current, bool hasDefault);

    RentalSystem& sys_;
    ConsoleIO& io_;
    const DataStore* store_;
    bool unsaved_ = false;
};

} // namespace rental
