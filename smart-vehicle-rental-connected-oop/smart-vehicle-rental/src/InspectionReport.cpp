#include "rental/InspectionReport.h"

#include "rental/Errors.h"

namespace rental {

InspectionReport::InspectionReport(Date date, long long odometerKm, int fuelPercent, std::string notes)
    : date_(date), odometerKm_(odometerKm), fuelPercent_(fuelPercent), notes_(std::move(notes)) {
    if (odometerKm_ < 0) throw ValidationError("Odometer reading cannot be negative.");
    if (fuelPercent_ < 0 || fuelPercent_ > 100) throw ValidationError("Fuel level must be 0-100%.");
    if (notes_.size() > 1000) throw ValidationError("Inspection notes are too long (max 1000 characters).");
}

} // namespace rental
