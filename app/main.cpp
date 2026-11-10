// Entry point (milestones 3-4): in-memory data, vehicles, customers, quotes and bookings.
#include "ConsoleApp.h"
#include "ConsoleIO.h"
#include "rental/RentalSystem.h"

#include <iostream>

int main() {
    using namespace rental;
    SystemClock clock;
    RentalSystem system(RentalData{}, clock);
    ConsoleIO io(std::cin, std::cout);
    ConsoleApp app(system, io);
    app.run();
    return 0;
}
