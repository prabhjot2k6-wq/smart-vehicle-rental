#pragma once
// Concrete vehicle types. Each one is a genuinely different kind of vehicle
// with its own extra attribute, deposit rule and service interval - this is
// why inheritance is used here (and NOT for brands such as "Swift").

#include "rental/Vehicle.h"

#include <memory>

namespace rental {

class Car final : public Vehicle {
public:
    Car(const VehicleData& data, int seats);
    VehicleCategory category() const override { return VehicleCategory::Car; }
    Money securityDeposit() const override;
    long long serviceIntervalKm() const override { return 10000; }
    std::string specLabel() const override { return "Seats"; }
    int specValue() const override { return seats_; }
    void setSpecValue(int seats) override;

private:
    int seats_ = 0;
};

class Bike final : public Vehicle {
public:
    Bike(const VehicleData& data, int engineCc);
    VehicleCategory category() const override { return VehicleCategory::Bike; }
    Money securityDeposit() const override; // higher for bikes above 350 cc
    long long serviceIntervalKm() const override { return 3000; }
    std::string specLabel() const override { return "Engine cc"; }
    int specValue() const override { return engineCc_; }
    void setSpecValue(int cc) override;

private:
    int engineCc_ = 0;
};

class Van final : public Vehicle {
public:
    Van(const VehicleData& data, int payloadKg);
    VehicleCategory category() const override { return VehicleCategory::Van; }
    Money securityDeposit() const override;
    long long serviceIntervalKm() const override { return 8000; }
    std::string specLabel() const override { return "Payload kg"; }
    int specValue() const override { return payloadKg_; }
    void setSpecValue(int kg) override;

private:
    int payloadKg_ = 0;
};

// Factory: builds the right derived type from a category. Used when adding a
// vehicle and when reloading from the data file.
std::unique_ptr<Vehicle> makeVehicle(VehicleCategory category, const VehicleData& data, int specValue);

} // namespace rental
