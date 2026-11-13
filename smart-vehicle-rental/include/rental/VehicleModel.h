#pragma once
// A vehicle *model* (e.g. "Maruti Swift"), shared by many physical vehicles.
//
// Brand/model data lives here instead of in subclasses: we do NOT create a
// class per brand. Adding "Tata Nexon" is just a new VehicleModel object,
// not new code. The base daily rate is per model and drives pricing.

#include "rental/Enums.h"
#include "rental/Money.h"

#include <string>

namespace rental {

class VehicleModel {
public:
    VehicleModel(std::string id, std::string brand, std::string name, VehicleCategory category,
                 Money baseDailyRate);

    const std::string& id() const { return id_; }
    const std::string& brand() const { return brand_; }
    const std::string& name() const { return name_; }
    std::string displayName() const { return brand_ + " " + name_; }
    VehicleCategory category() const { return category_; }
    Money baseDailyRate() const { return baseDailyRate_; }

    void rename(const std::string& brand, const std::string& name);
    // Affects future quotes only; confirmed bookings keep their stored price.
    void setBaseDailyRate(Money rate);

private:
    std::string id_;
    std::string brand_;
    std::string name_;
    VehicleCategory category_;
    Money baseDailyRate_;
};

} // namespace rental
