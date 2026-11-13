#include "rental/VehicleModel.h"

#include "rental/Errors.h"
#include "rental/TextUtil.h"

namespace rental {

VehicleModel::VehicleModel(std::string id, std::string brand, std::string name, VehicleCategory category,
                           Money baseDailyRate)
    : id_(requireText(id, "Model ID")), category_(category) {
    rename(brand, name);
    setBaseDailyRate(baseDailyRate);
}

void VehicleModel::rename(const std::string& brand, const std::string& name) {
    brand_ = requireText(brand, "Brand");
    name_ = requireText(name, "Model name");
}

void VehicleModel::setBaseDailyRate(Money rate) {
    if (rate <= Money()) throw ValidationError("Base daily rate must be greater than zero.");
    if (rate > Money::fromRupees(1000000)) throw ValidationError("Base daily rate is unrealistically high.");
    baseDailyRate_ = rate;
}

} // namespace rental
