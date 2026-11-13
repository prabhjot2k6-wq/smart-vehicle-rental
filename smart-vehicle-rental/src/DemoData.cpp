#include "rental/DemoData.h"

namespace rental {

namespace {

const Booking& book(RentalSystem& sys, const std::string& customerId, const std::string& vehicleId, Date start,
                    Date end) {
    const std::string quoteId = sys.quotes().createQuote(customerId, vehicleId, DateRange(start, end)).id;
    return sys.rentals().confirmQuote(quoteId);
}

} // namespace

void populateDemoData(RentalSystem& sys, ManualClock& clock, Date today) {
    const auto day = [today](long long offset) { return today.addDays(offset); };
    clock.setToday(day(-20));
    FleetService& fleet = sys.fleet();

    // ---- models (synthetic rates) ----
    const std::string swift = fleet.addModel("Maruti", "Swift", VehicleCategory::Car, Money::fromRupees(1500)).id();
    const std::string creta = fleet.addModel("Hyundai", "Creta", VehicleCategory::Car, Money::fromRupees(2500)).id();
    const std::string innova = fleet.addModel("Toyota", "Innova", VehicleCategory::Car, Money::fromRupees(3500)).id();
    const std::string activa = fleet.addModel("Honda", "Activa", VehicleCategory::Bike, Money::fromRupees(500)).id();
    const std::string classic =
        fleet.addModel("Royal Enfield", "Classic 350", VehicleCategory::Bike, Money::fromRupees(900)).id();
    const std::string eeco = fleet.addModel("Maruti", "Eeco Cargo", VehicleCategory::Van, Money::fromRupees(1800)).id();

    // ---- vehicles (fake "DEMO" registrations) ----
    std::vector<std::string> swifts;
    for (int i = 1; i <= 4; ++i) {
        swifts.push_back(fleet.addVehicle(swift, "DM01SW000" + std::to_string(i), FuelType::Petrol, 8000 + 1500 * i, 5).id());
    }
    const std::string creta1 = fleet.addVehicle(creta, "DM01CR0001", FuelType::Diesel, 21000, 5).id();
    const std::string creta2 = fleet.addVehicle(creta, "DM01CR0002", FuelType::Petrol, 33500, 5).id();
    const std::string innova1 = fleet.addVehicle(innova, "DM01IN0001", FuelType::Diesel, 45200, 7).id();
    const std::string innova2 = fleet.addVehicle(innova, "DM01IN0002", FuelType::Diesel, 12100, 7).id();
    const std::string activa1 = fleet.addVehicle(activa, "DM01AC0001", FuelType::Petrol, 5400, 110).id();
    fleet.addVehicle(activa, "DM01AC0002", FuelType::Electric, 2100, 110);
    fleet.addVehicle(classic, "DM01RE0001", FuelType::Petrol, 7300, 349);
    const std::string van = fleet.addVehicle(eeco, "DM01EC0001", FuelType::CNG, 15700, 700).id();
    fleet.updateNextServiceDue(van, 16000); // 300 km left -> service WARNING (still bookable)

    // ---- customers (synthetic people, example.com addresses) ----
    CustomerService& cs = sys.customers();
    const std::string aarav = cs.registerCustomer("Aarav Sharma", "+91 90000 00101", "aarav.demo@example.com",
                                                  DrivingLicence("DEMO-DL-0001", Date::fromYmd(2030, 6, 30),
                                                                 {VehicleCategory::Car, VehicleCategory::Bike}))
                                  .id();
    const std::string priya = cs.registerCustomer("Priya Nair", "+91 90000 00102", "priya.demo@example.com",
                                                  DrivingLicence("DEMO-DL-0002", Date::fromYmd(2031, 1, 15),
                                                                 {VehicleCategory::Car, VehicleCategory::Van}))
                                  .id();
    const std::string rohan = cs.registerCustomer("Rohan Gupta", "+91 90000 00103", "rohan.demo@example.com",
                                                  DrivingLicence("DEMO-DL-0003", Date::fromYmd(2029, 9, 1),
                                                                 {VehicleCategory::Bike}))
                                  .id();
    // Licence expires soon: good for demonstrating the validity check.
    const std::string meera = cs.registerCustomer("Meera Iyer", "+91 90000 00104", "meera.demo@example.com",
                                                  DrivingLicence("DEMO-DL-0004", day(12), {VehicleCategory::Car}))
                                  .id();

    // ---- holidays ----
    PricingRules& rules = sys.data().pricingRules;
    rules.addHoliday(Date::fromYmd(2026, 12, 25));
    rules.addHoliday(Date::fromYmd(2027, 1, 1));
    rules.addHoliday(Date::fromYmd(2027, 1, 26));
    rules.addHoliday(day(5)); // a demo holiday close to "today"

    // ---- history: a completed Creta rental, returned late with fuel and damage charges ----
    clock.setToday(day(-15));
    const std::string past = book(sys, priya, creta1, day(-15), day(-12)).id();
    sys.rentals().pickup(past, InspectionReport(day(-15), 21000, 100, "Clean. Small scuff on rear bumper (pre-existing)."));
    clock.setToday(day(-11));
    sys.rentals().completeReturn(past, InspectionReport(day(-11), 21950, 75, "Left wing mirror cracked."),
                                 {DamageCharge("Cracked left wing mirror", Money::fromRupees(1200))});

    // ---- history: a completed bike rental, on time ----
    clock.setToday(day(-10));
    const std::string bikeTrip = book(sys, rohan, activa1, day(-10), day(-8)).id();
    sys.rentals().pickup(bikeTrip, InspectionReport(day(-10), 5400, 100, ""));
    clock.setToday(day(-8));
    sys.rentals().completeReturn(bikeTrip, InspectionReport(day(-8), 5610, 100, "OK"), {});

    // ---- completed maintenance on Swift #4 ----
    const std::string mt = sys.maintenance().openMaintenance(swifts[3], "Routine service and oil change", day(-7)).id();
    sys.maintenance().completeMaintenance(mt, day(-6), 14000, Money::fromRupees(3800));

    // ---- an active rental: Swift #1 collected 2 days ago ----
    clock.setToday(day(-2));
    const std::string active = book(sys, aarav, swifts[0], day(-2), day(3)).id();
    sys.rentals().pickup(active, InspectionReport(day(-2), 9500, 100, "Clean"));

    // ---- today: future bookings ----
    clock.setToday(today);
    book(sys, priya, innova2, day(0), day(2));            // ready for a pickup demo today
    book(sys, meera, swifts[0], day(7), day(10));          // three of four Swifts booked next week:
    book(sys, priya, swifts[1], day(7), day(10));          //   a quote for the fourth shows the
    book(sys, aarav, swifts[2], day(7), day(10));          //   70-90% demand tier (1.15x)
    const std::string cancelled = book(sys, aarav, innova1, day(2), day(4)).id();
    sys.rentals().cancelBooking(cancelled);

    // ---- a vehicle under maintenance right now (blocked) ----
    sys.maintenance().openMaintenance(creta2, "Brake pad replacement", day(-1));
}

} // namespace rental
