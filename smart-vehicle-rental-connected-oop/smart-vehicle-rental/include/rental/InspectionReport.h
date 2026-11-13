#pragma once
// What staff record when a vehicle is handed over or brought back.
// Notes are free text for humans; the system does NOT try to infer damage
// from them - damage charges are entered explicitly with a reason.

#include "rental/Date.h"

#include <string>

namespace rental {

class InspectionReport {
public:
    // fuelPercent: tank/battery level 0-100. Throws ValidationError.
    InspectionReport(Date date, long long odometerKm, int fuelPercent, std::string notes);

    Date date() const { return date_; }
    long long odometerKm() const { return odometerKm_; }
    int fuelPercent() const { return fuelPercent_; }
    const std::string& notes() const { return notes_; }

private:
    Date date_;
    long long odometerKm_;
    int fuelPercent_;
    std::string notes_;
};

} // namespace rental
