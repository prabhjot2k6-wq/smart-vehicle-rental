#pragma once
// Abstract base class for one physical vehicle (one registration plate).
//
// OOP notes for the viva:
//  * ABSTRACTION: Vehicle has pure virtual functions, so you cannot create a
//    "plain" Vehicle - only a Car, Bike or Van.
//  * ENCAPSULATION: all data is private; it changes only through methods that
//    validate (mileage can never go down, registration is normalised...).
//  * POLYMORPHISM: code holding a Vehicle& calls securityDeposit() or
//    serviceIntervalKm() and gets the Car/Bike/Van answer at run time.
//  * Copying is disabled: copying through a base reference would "slice" the
//    object. Vehicles are owned by std::unique_ptr in VehicleRepository.

#include "rental/Enums.h"
#include "rental/Money.h"

#include <string>

namespace rental {

class VehicleModel;

// Plain data used to construct (or reload) a vehicle.
struct VehicleData {
    std::string id;
    std::string registration;
    std::string modelId;
    FuelType fuelType = FuelType::Petrol;
    long long mileageKm = 0;
    VehicleStatus status = VehicleStatus::Active;
    long long nextServiceDueKm = 0;
};

class Vehicle {
public:
    virtual ~Vehicle() = default;
    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    // ---- category-specific behaviour (implemented by Car / Bike / Van) ----
    virtual VehicleCategory category() const = 0;
    virtual Money securityDeposit() const = 0;      // refundable deposit taken at pickup
    virtual long long serviceIntervalKm() const = 0; // km between routine services
    virtual std::string specLabel() const = 0;     // "Seats", "Engine cc", "Payload kg"
    virtual int specValue() const = 0;
    virtual void setSpecValue(int value) = 0;      // validated per type

    std::string specSummary() const { return specLabel() + ": " + std::to_string(specValue()); }

    // ---- common state ----
    const std::string& id() const { return id_; }
    const std::string& registration() const { return registration_; }
    const std::string& modelId() const { return modelId_; }
    FuelType fuelType() const { return fuelType_; }
    long long mileageKm() const { return mileageKm_; }
    VehicleStatus status() const { return status_; }
    long long nextServiceDueKm() const { return nextServiceDueKm_; }
    long long kmUntilService() const { return nextServiceDueKm_ - mileageKm_; }

    // Only Active vehicles count as rentable fleet capacity.
    bool isRentable() const { return status_ == VehicleStatus::Active; }

    // ---- validated operations ----
    void setRegistration(const std::string& registration);
    void setFuelType(FuelType fuel) { fuelType_ = fuel; }
    void recordOdometer(long long km);              // never decreases
    void setNextServiceDueKm(long long km);
    void startMaintenance();                        // Active -> UnderMaintenance
    void finishMaintenance();                       // UnderMaintenance -> Active
    void deactivate();                              // any -> Inactive (history kept)
    void reactivate();                              // Inactive -> Active

    // "mh-12 ab 1234" -> "MH12AB1234"; throws if not 4..15 letters/digits.
    static std::string normalizeRegistration(const std::string& raw);

protected:
    explicit Vehicle(const VehicleData& data);

private:
    std::string id_;
    std::string registration_;
    std::string modelId_;
    FuelType fuelType_;
    long long mileageKm_;
    VehicleStatus status_;
    long long nextServiceDueKm_;
};

} // namespace rental
