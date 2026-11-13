// Tests for vehicle and customer management services.

#include "TestFramework.h"

#include "TestHelpers.h"

using namespace rental;

TEST_CASE("fleet: IDs are generated and duplicate registrations rejected") {
    ManualClock clock(Date::parse("2026-11-09"));
    RentalSystem sys(RentalData{}, clock);
    const auto& swift = sys.fleet().addModel("Maruti", "Swift", VehicleCategory::Car, Money::fromRupees(1500));
    CHECK_EQ(swift.id(), std::string("M-0001"));
    const auto& v1 = sys.fleet().addVehicle(swift.id(), "MH12 AB 1001", FuelType::Petrol, 100, 5);
    CHECK_EQ(v1.id(), std::string("V-0001"));
    CHECK_EQ(v1.nextServiceDueKm(), 10100LL);
    CHECK_THROWS_AS(sys.fleet().addVehicle(swift.id(), "mh-12-ab-1001", FuelType::Petrol, 0, 5), ConflictError);
    // A failed add must not consume an ID.
    const auto& v2 = sys.fleet().addVehicle(swift.id(), "MH12AB1002", FuelType::Petrol, 0, 5);
    CHECK_EQ(v2.id(), std::string("V-0002"));
    CHECK_THROWS_AS(sys.fleet().updateRegistration("V-0002", "MH12AB1001"), ConflictError);
    CHECK_THROWS_AS(sys.fleet().addModel("Maruti", "Swift", VehicleCategory::Car, Money::fromRupees(1)), ConflictError);
    CHECK_THROWS_AS(sys.fleet().addVehicle("M-0099", "XX11", FuelType::Petrol, 0, 5), NotFoundError);
}

TEST_CASE("fleet: vehicle type follows the model category, many vehicles share one model") {
    ManualClock clock(Date::parse("2026-11-09"));
    RentalSystem sys(RentalData{}, clock);
    const auto& activa = sys.fleet().addModel("Honda", "Activa", VehicleCategory::Bike, Money::fromRupees(500));
    const auto& b1 = sys.fleet().addVehicle(activa.id(), "KA01BK0001", FuelType::Petrol, 0, 110);
    const auto& b2 = sys.fleet().addVehicle(activa.id(), "KA01BK0002", FuelType::Petrol, 0, 110);
    CHECK(b1.category() == VehicleCategory::Bike);
    CHECK_EQ(b1.modelId(), b2.modelId());
    CHECK_THROWS_AS(sys.fleet().addVehicle(activa.id(), "KA01BK0003", FuelType::Petrol, 0, 5), ValidationError);
}

TEST_CASE("customers: unique licence numbers") {
    ManualClock clock(Date::parse("2026-11-09"));
    RentalSystem sys(RentalData{}, clock);
    const DrivingLicence lic("DEMO-1", Date::parse("2030-01-01"), {VehicleCategory::Car});
    const auto& c = sys.customers().registerCustomer("Asha Rao", "+91 90000 00001", "asha@example.com", lic);
    CHECK_EQ(c.id(), std::string("C-0001"));
    CHECK_THROWS_AS(sys.customers().registerCustomer("Other", "+91 90000 00002", "o@example.com",
                                                     DrivingLicence("demo-1", Date::parse("2030-01-01"),
                                                                    {VehicleCategory::Bike})),
                    ConflictError);
}
