#include "rental/VehicleTypes.h"

#include "rental/Errors.h"

namespace rental {

// ---------------- Car ----------------
Car::Car(const VehicleData& data, int seats) : Vehicle(data) { setSpecValue(seats); }

Money Car::securityDeposit() const { return Money::fromRupees(5000); }

void Car::setSpecValue(int seats) {
    if (seats < 2 || seats > 9) throw ValidationError("A car must have 2-9 seats.");
    seats_ = seats;
}

// ---------------- Bike ----------------
Bike::Bike(const VehicleData& data, int engineCc) : Vehicle(data) { setSpecValue(engineCc); }

Money Bike::securityDeposit() const {
    return engineCc_ > 350 ? Money::fromRupees(3000) : Money::fromRupees(2000);
}

void Bike::setSpecValue(int cc) {
    if (cc < 50 || cc > 2000) throw ValidationError("Bike engine size must be 50-2000 cc.");
    engineCc_ = cc;
}

// ---------------- Van ----------------
Van::Van(const VehicleData& data, int payloadKg) : Vehicle(data) { setSpecValue(payloadKg); }

Money Van::securityDeposit() const { return Money::fromRupees(8000); }

void Van::setSpecValue(int kg) {
    if (kg < 100 || kg > 5000) throw ValidationError("Van payload must be 100-5000 kg.");
    payloadKg_ = kg;
}

// ---------------- Factory ----------------
std::unique_ptr<Vehicle> makeVehicle(VehicleCategory category, const VehicleData& data, int specValue) {
    switch (category) {
    case VehicleCategory::Car: return std::make_unique<Car>(data, specValue);
    case VehicleCategory::Bike: return std::make_unique<Bike>(data, specValue);
    case VehicleCategory::Van: return std::make_unique<Van>(data, specValue);
    }
    throw ValidationError("Unknown vehicle category.");
}

} // namespace rental
