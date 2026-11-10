// Entry point.
//
//   rental_app                      start with the data file (default data/rental_data.txt)
//   rental_app --seed-demo          create synthetic demo data, then start
//   rental_app --data other.txt     use another data file
//
// Startup never overwrites a data file it cannot read: a corrupted file is
// reported and the program exits, leaving the file (and its .bak) untouched.

#include "ConsoleApp.h"
#include "ConsoleIO.h"
#include "rental/DataStore.h"
#include "rental/DemoData.h"
#include "rental/Errors.h"
#include "rental/RentalSystem.h"

#include <iostream>
#include <string>

namespace {

void printUsage() {
    std::cout << "Usage: rental_app [--data <file>] [--seed-demo]\n"
                 "  --data <file>  data file to use (default: data/rental_data.txt)\n"
                 "  --seed-demo    create synthetic demo data in a NEW data file, then start\n";
}

} // namespace

int main(int argc, char** argv) {
    using namespace rental;
    std::string dataFile = "data/rental_data.txt";
    bool seedDemo = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) {
            dataFile = argv[++i];
        } else if (arg == "--seed-demo") {
            seedDemo = true;
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage();
            return 2;
        }
    }

    const DataStore store(dataFile);
    SystemClock clock;

    try {
        if (seedDemo) {
            if (store.exists() || std::filesystem::exists(store.backupPath())) {
                std::cerr << "Refusing to seed: " << store.path().string()
                          << " (or its .bak) already exists. Seeding only creates a new file,\n"
                             "so demo records are never duplicated. Use --data <new file> or delete the old file.\n";
                return 2;
            }
            ManualClock seedClock(clock.today());
            RentalSystem seedSystem(RentalData{}, seedClock);
            populateDemoData(seedSystem, seedClock, clock.today());
            store.save(seedSystem.data());
            std::cout << "Demo data created in " << store.path().string() << ".\n";
        }

        RentalData data;
        if (store.exists()) {
            data = store.load();
        } else if (std::filesystem::exists(store.backupPath())) {
            std::cerr << "Data file " << store.path().string() << " is missing but a backup exists at "
                      << store.backupPath().string() << ".\nA previous save may have been interrupted. "
                      << "Copy the .bak file to " << store.path().string() << " to restore it, then restart.\n";
            return 2;
        } else {
            std::cout << "No data file yet - starting empty. (Tip: run with --seed-demo for sample data.)\n";
        }

        RentalSystem system(std::move(data), clock);
        ConsoleIO io(std::cin, std::cout);
        ConsoleApp app(system, io, &store);
        app.run();
    } catch (const PersistenceError& e) {
        std::cerr << "\nCannot load data: " << e.what()
                  << "\nThe file has NOT been changed. Fix it or restore the .bak copy, then restart.\n";
        return 2;
    } catch (const RentalError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
