// Milestone 1 entry point: proves the domain objects compile and behave.
#include "rental/Customer.h"
#include "rental/VehicleModel.h"
#include "rental/VehicleTypes.h"

#include <iostream>

int main() {
    using namespace rental;
    const VehicleModel swift("M-0001", "Maruti", "Swift", VehicleCategory::Car, Money::fromRupees(1500));
    VehicleData data;
    data.id = "V-0001";
    data.registration = "MH12 AB 1001";
    data.modelId = swift.id();
    data.mileageKm = 12000;
    auto car = makeVehicle(VehicleCategory::Car, data, 5);

    std::cout << "Smart Vehicle Rental - milestone 1\n"
              << car->id() << " " << car->registration() << " " << swift.displayName() << " ("
              << toString(car->category()) << ", " << car->specSummary() << ")\n"
              << "Base rate: " << swift.baseDailyRate() << "/day, deposit: " << car->securityDeposit() << "\n";
    return 0;
}
