#include "rental/Vehicle.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

#include <cctype>

namespace rental {

Vehicle::Vehicle(const VehicleData& data)
    : id_(requireText(data.id, "Vehicle ID")),
      registration_(normalizeRegistration(data.registration)),
      modelId_(requireText(data.modelId, "Model ID")),
      fuelType_(data.fuelType),
      mileageKm_(data.mileageKm),
      status_(data.status),
      nextServiceDueKm_(data.nextServiceDueKm) {
    if (mileageKm_ < 0) throw ValidationError("Mileage cannot be negative.");
    if (nextServiceDueKm_ < 0) throw ValidationError("Next service mileage cannot be negative.");
}

std::string Vehicle::normalizeRegistration(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (c == ' ' || c == '-') continue;
        if (!std::isalnum(uc)) {
            throw ValidationError("Registration may contain only letters, digits, spaces and '-' (got '" + raw + "').");
        }
        out += static_cast<char>(std::toupper(uc));
    }
    if (out.size() < 4 || out.size() > 15) {
        throw ValidationError("Registration must have 4-15 letters/digits (got '" + raw + "').");
    }
    return out;
}

void Vehicle::setRegistration(const std::string& registration) {
    registration_ = normalizeRegistration(registration);
}

void Vehicle::recordOdometer(long long km) {
    if (km < mileageKm_) {
        throw ValidationError("Odometer reading " + std::to_string(km) + " km is lower than the recorded " +
                              std::to_string(mileageKm_) + " km for " + id_ + ".");
    }
    mileageKm_ = km;
}

void Vehicle::setNextServiceDueKm(long long km) {
    if (km <= 0) throw ValidationError("Next service mileage must be positive.");
    nextServiceDueKm_ = km;
}

void Vehicle::startMaintenance() {
    if (status_ != VehicleStatus::Active) {
        throw ConflictError("Vehicle " + id_ + " is " + toString(status_) + "; only Active vehicles can enter maintenance.");
    }
    status_ = VehicleStatus::UnderMaintenance;
}

void Vehicle::finishMaintenance() {
    if (status_ == VehicleStatus::UnderMaintenance) status_ = VehicleStatus::Active;
    // An Inactive vehicle stays inactive after its maintenance record closes.
}

void Vehicle::deactivate() {
    if (status_ == VehicleStatus::Inactive) throw ConflictError("Vehicle " + id_ + " is already inactive.");
    status_ = VehicleStatus::Inactive;
}

void Vehicle::reactivate() {
    if (status_ != VehicleStatus::Inactive) throw ConflictError("Vehicle " + id_ + " is not inactive.");
    status_ = VehicleStatus::Active;
}

} // namespace rental
