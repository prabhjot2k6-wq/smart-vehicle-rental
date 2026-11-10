#include "rental/RentalSystem.h"

#include <utility>

namespace rental {

RentalSystem::RentalSystem(RentalData data) : data_(std::move(data)), fleet_(data_), customers_(data_) {}

} // namespace rental
