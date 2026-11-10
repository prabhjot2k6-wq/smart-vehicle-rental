#pragma once
// FleetService: vehicle models and the vehicles themselves.
// Responsible for: unique IDs/registrations, matching vehicle type to model
// category, and safe deactivation (history is never deleted).

#include "rental/RentalData.h"

#include <string>
#include <vector>

namespace rental {

class FleetService {
public:
    explicit FleetService(RentalData& data) : data_(data) {}

    const VehicleModel& addModel(const std::string& brand, const std::string& name, VehicleCategory category,
                                 Money baseDailyRate);
    void renameModel(const std::string& modelId, const std::string& brand, const std::string& name);
    // Changes future quotes only.
    void setModelRate(const std::string& modelId, Money baseDailyRate);

    // The vehicle's type (Car/Bike/Van) comes from the model's category.
    const Vehicle& addVehicle(const std::string& modelId, const std::string& registration, FuelType fuel,
                              long long mileageKm, int specValue);
    void updateRegistration(const std::string& vehicleId, const std::string& registration);
    void updateFuelType(const std::string& vehicleId, FuelType fuel);
    void updateSpec(const std::string& vehicleId, int specValue);
    void updateNextServiceDue(const std::string& vehicleId, long long km);

    // Refuses while the vehicle has reserved or ongoing bookings.
    void deactivateVehicle(const std::string& vehicleId);
    void reactivateVehicle(const std::string& vehicleId);

    const VehicleModel& modelOf(const Vehicle& vehicle) const { return data_.models.get(vehicle.modelId()); }
    std::vector<const VehicleModel*> models() const { return data_.models.all(); }
    std::vector<const Vehicle*> vehicles() const { return data_.vehicles.all(); }
    const Vehicle& vehicle(const std::string& id) const { return data_.vehicles.get(id); }

private:
    RentalData& data_;
};

} // namespace rental
