// Entry point (milestone 2): in-memory data, vehicle and customer menus.
#include "ConsoleApp.h"
#include "ConsoleIO.h"
#include "rental/RentalSystem.h"

#include <iostream>

int main() {
    using namespace rental;
    RentalSystem system{RentalData{}};
    ConsoleIO io(std::cin, std::cout);
    ConsoleApp app(system, io);
    app.run();
    return 0;
}
