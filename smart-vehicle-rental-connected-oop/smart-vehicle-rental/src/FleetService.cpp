#include "rental/FleetService.h"

#include "rental/VehicleTypes.h"

namespace rental {

const VehicleModel& FleetService::addModel(const std::string& brand, const std::string& name,
                                           VehicleCategory category, Money baseDailyRate) {
    for (const VehicleModel* m : data_.models.all()) {
        if (m->brand() == brand && m->name() == name) {
            throw ConflictError("Model " + m->displayName() + " already exists as " + m->id() + ".");
        }
    }
    // Construct (and validate) before consuming an ID.
    VehicleModel candidate("pending", brand, name, category, baseDailyRate);
    return data_.models.add(VehicleModel(data_.ids.next("M"), candidate.brand(), candidate.name(), category,
                                         baseDailyRate));
}

void FleetService::renameModel(const std::string& modelId, const std::string& brand, const std::string& name) {
    data_.models.get(modelId).rename(brand, name);
}

void FleetService::setModelRate(const std::string& modelId, Money baseDailyRate) {
    data_.models.get(modelId).setBaseDailyRate(baseDailyRate);
}

const Vehicle& FleetService::addVehicle(const std::string& modelId, const std::string& registration, FuelType fuel,
                                        long long mileageKm, int specValue) {
    const VehicleModel& model = data_.models.get(modelId);
    VehicleData d;
    d.id = "pending";
    d.registration = registration;
    d.modelId = modelId;
    d.fuelType = fuel;
    d.mileageKm = mileageKm;
    d.status = VehicleStatus::Active;

    // Validate fully (including duplicate registration) before using an ID.
    auto probe = VehicleFactory::instance().create(model.category(), d, specValue);
    if (const Vehicle* other = data_.vehicles.findByRegistration(probe->registration())) {
        throw ConflictError("Registration " + probe->registration() + " is already used by vehicle " + other->id() +
                            ".");
    }
    d.id = data_.ids.next("V");
    d.nextServiceDueKm = mileageKm + probe->serviceIntervalKm();
    return data_.vehicles.add(VehicleFactory::instance().create(model.category(), d, specValue));
}

void FleetService::updateRegistration(const std::string& vehicleId, const std::string& registration) {
    Vehicle& v = data_.vehicles.get(vehicleId);
    const Vehicle* other = data_.vehicles.findByRegistration(registration);
    if (other != nullptr && other != &v) {
        throw ConflictError("Registration " + other->registration() + " is already used by vehicle " + other->id() + ".");
    }
    v.setRegistration(registration);
}

void FleetService::updateFuelType(const std::string& vehicleId, FuelType fuel) {
    data_.vehicles.get(vehicleId).setFuelType(fuel);
}

void FleetService::updateSpec(const std::string& vehicleId, int specValue) {
    data_.vehicles.get(vehicleId).setSpecValue(specValue);
}

void FleetService::updateNextServiceDue(const std::string& vehicleId, long long km) {
    data_.vehicles.get(vehicleId).setNextServiceDueKm(km);
}

void FleetService::deactivateVehicle(const std::string& vehicleId) {
    Vehicle& v = data_.vehicles.get(vehicleId);
    for (const Booking* b : data_.bookings.all()) {
        if (b->vehicleId() == vehicleId && b->occupiesVehicle()) {
            throw ConflictError("Vehicle " + vehicleId + " has " + toString(b->status()) + " booking " + b->id() +
                                ". Cancel or complete it before deactivating.");
        }
    }
    // Only the status changes: past bookings and invoices stay untouched.
    v.deactivate();
}

void FleetService::reactivateVehicle(const std::string& vehicleId) {
    data_.vehicles.get(vehicleId).reactivate();
}

} // namespace rental
