#include "rental/RentalSystem.h"

#include <utility>

namespace rental {

RentalSystem::RentalSystem(RentalData data, const Clock& clock)
    : data_(std::move(data)),
      clock_(clock),
      fleet_(data_),
      customers_(data_),
      availability_(data_),
      quotes_(data_, availability_, clock_, std::make_unique<DynamicPricingStrategy>()),
      charges_(data_.chargeRules),
      rentals_(data_, availability_, quotes_, charges_, clock_),
      maintenance_(data_, availability_),
      reports_(data_, availability_) {}

} // namespace rental
