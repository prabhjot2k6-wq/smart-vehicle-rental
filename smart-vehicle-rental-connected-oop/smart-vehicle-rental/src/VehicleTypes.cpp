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

VehicleFactory::VehicleFactory() {
    registerCreator(VehicleCategory::Car,
                    [](const VehicleData& d, int spec) { return std::make_unique<Car>(d, spec); });
    registerCreator(VehicleCategory::Bike,
                    [](const VehicleData& d, int spec) { return std::make_unique<Bike>(d, spec); });
    registerCreator(VehicleCategory::Van,
                    [](const VehicleData& d, int spec) { return std::make_unique<Van>(d, spec); });
}

VehicleFactory& VehicleFactory::instance() {
    static VehicleFactory factory;
    return factory;
}

void VehicleFactory::registerCreator(VehicleCategory category, Creator creator) {
    if (!creator) throw ValidationError("A vehicle creator function is required.");
    creators_[category] = std::move(creator);
}

std::unique_ptr<Vehicle> VehicleFactory::create(VehicleCategory category, const VehicleData& data,
                                                int specValue) const {
    auto it = creators_.find(category);
    if (it == creators_.end()) throw ValidationError("Unknown vehicle category.");
    return it->second(data, specValue);
}

std::unique_ptr<Vehicle> makeVehicle(VehicleCategory category, const VehicleData& data, int specValue) {
    return VehicleFactory::instance().create(category, data, specValue);
}

} // namespace rental
