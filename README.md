# Smart Vehicle Rental Management System

A menu-driven C++17 console application for an Object-Oriented Programming course.
It manages a fleet of cars, bikes and vans, customers with driving licences,
quotes with **model-specific dynamic pricing**, bookings, pickups, returns with
itemised invoices, maintenance, reports, and safe local file storage.

> Educational project: all customer data is synthetic and all payments are simulated.

| Document | Contents |
|---|---|
| `README.md` (this file) | Setup, build, test, usage, assumptions |
| [`docs/DESIGN.md`](docs/DESIGN.md) | Architecture, class diagram (Mermaid), OOP mapping, pricing rules and a worked example, file format |
| [`docs/VIVA_GUIDE.md`](docs/VIVA_GUIDE.md) | Five-minute demo script, likely viva questions, limitations and future work |

---

## 1. Requirements

* A C++17 compiler: GCC 9+, Clang 10+ (Apple Clang on macOS 10.15+, needed for
  `<filesystem>`), or Visual Studio 2019 (16.8)+ / MSVC 19.28+
* CMake 3.16+ (optional - a direct `g++` command is given below)
* No other libraries. Only the C++ standard library is used.

## 2. Build

### With CMake (Windows, Linux, macOS)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Executables: `build/rental_app` and `build/rental_tests`
(on Visual Studio: `build/Release/rental_app.exe`, `build/Release/rental_tests.exe`).

### Directly with g++ (no CMake)

Run from the project folder:

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude src/*.cpp app/*.cpp -o rental_app
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude src/*.cpp tests/*.cpp -o rental_tests
```

On Windows with MinGW, run the same commands in Git Bash or MSYS2 (they expand
`*.cpp`); from `cmd.exe` or PowerShell the CMake build is the simpler option.
(GCC 8 additionally needs `-lstdc++fs` for `<filesystem>`; GCC 9+ does not.)

## 3. Test

```bash
ctest --test-dir build --output-on-failure      # via CTest
./build/rental_tests                             # run directly, shows every test
./build/rental_tests pricing                     # only tests whose name contains "pricing"
```

`--test-dir` needs CMake 3.20+. On older CMake: `cd build && ctest --output-on-failure`.
With Visual Studio add `-C Release`.

**Verified in the development environment** (Linux, GCC 13.3, CMake 3.28):
CMake build with zero compiler warnings, CTest passing (it runs `rental_tests`,
which contains 49 test cases, all passing), the direct
`g++` commands above, a clang++ warning check, and a test run under
AddressSanitizer + UndefinedBehaviorSanitizer. Windows and macOS builds were
**not** run; the code uses only portable standard C++ (the one OS-specific call,
`localtime_s` / `localtime_r`, is guarded by `#if defined(_WIN32)`).

## 4. Run

```bash
./build/rental_app --seed-demo     # first time: create synthetic demo data, then start
./build/rental_app                 # later runs: load the saved data
./build/rental_app --data my.txt   # use a different data file
```

* Data is stored in `data/rental_data.txt` (relative to where you run the program).
  It is saved automatically after every successful change, and again on exit.
* `--seed-demo` **refuses** to run if the data file (or its `.bak`) already exists,
  so restarting can never duplicate demo records. To reset the demo, delete the
  `data` folder and seed again.
* Type `cancel` at any prompt to abandon the current action. Press Enter on a
  prompt showing `[value]` to keep that value.

### Main menu

```
1. Vehicle management          models, base rates, add/update/deactivate vehicles
2. Customer management         register/update customers and licences
3. Availability search and price quotes
                               search by dates/category/model/budget, quotes,
                               pricing settings (strategy, holidays, factors)
4. Booking management          list, view (with price breakdown), extend, cancel
5. Pickup and return           collect, return with charges, invoices
6. Maintenance                 open/complete maintenance, service warnings
7. Reports                     fleet availability, active rentals, maintenance due,
                               revenue, booking history
8. Save and exit
```

## 5. Key rules and assumptions

**Dates (date-only rental model).** Dates are entered as `YYYY-MM-DD` and are
validated as real calendar dates (leap years included). A rental period is
`[pickup date, return date)`: the return date is not billed, so
`2026-11-12 -> 2026-11-15` is 3 billed days. One booking may end on the same day
the next one starts. **This version bills whole days only** - there are no hours
anywhere. The same rule is used for availability, pricing, extensions, mileage
allowance and late fees.

**Money.** Stored as integer paise (`int64`), never as `double`. Factors and
percentages are integer basis points (10000 = 1.00x = 100%). **Rounding rule:
half away from zero to the nearest paisa**, applied to each daily amount and to
the discount.

**Pricing** (all configurable; details and a worked example in `docs/DESIGN.md`):

| Rule | Default |
|---|---|
| Demand factor by model utilization that day | <30% 0.90 · 30-<70% 1.00 · 70-<90% 1.15 · >=90% 1.25 |
| Weekend (Sat/Sun) factor | 1.10 |
| Holiday factor | 1.15 (if a day is both, the **higher** one is used) |
| Cap on demand x calendar | 1.40 per day, before discounts |
| Duration discount | 7-29 days 10% · 30+ days 20% (only the highest) |
| Quote validity | 15 minutes |

**Licence.** Must permit the vehicle's category and be valid up to and including
the return date.

**Return charges** (configurable in `ChargeRules`):

| | Car | Bike | Van |
|---|---|---|---|
| Free km per billed day | 250 | 150 | 200 |
| Excess per km | INR 10 | INR 4 | INR 14 |
| Fuel/charge per missing % point | INR 60 | INR 15 | INR 90 |
| Refundable deposit | INR 5,000 | INR 2,000 (INR 3,000 above 350 cc) | INR 8,000 |

Late return: per day after the agreed return date, 1.5x the booking's original
base daily rate. Early return: no refund. Damage: entered manually with a reason
- never guessed from inspection notes. The deposit is shown separately on the
invoice and refunded in full (simulated); it is never counted as revenue.

**Other decisions.** No cancellation fee. Pending quotes live in memory only and
are lost on exit (they expire within minutes anyway). The pricing strategy
choice (Dynamic/Standard) resets to Dynamic on restart; all other settings are
saved. One copy of the program per data file (no file locking).

## 6. Project structure

```
CMakeLists.txt
include/rental/      public headers (domain model, services, persistence)
src/                 implementations (no console I/O here)
app/                 console UI: ConsoleIO (input/tables) + ConsoleApp (menus)
tests/               dependency-free test framework + 49 tests (CTest)
docs/                DESIGN.md, VIVA_GUIDE.md
```

| Layer | Files |
|---|---|
| Value types | `Money`, `Date`/`DateRange`, `Enums`, `Errors`, `TextUtil` |
| Domain | `VehicleModel`, `Vehicle` + `VehicleTypes` (Car/Bike/Van), `Customer`, `Booking`, `InspectionReport`, `Invoice`, `MaintenanceRecord`, `PricingRules`, `PriceBreakdown` |
| Pricing | `PricingStrategy` (interface, Standard, Dynamic, `PricingContext`) |
| Services | `FleetService`, `CustomerService`, `AvailabilityService`, `QuoteService`, `RentalService`, `ChargeCalculator`, `MaintenanceService`, `ReportService` |
| Storage | `Repository` (in-memory collections + `IdGenerator`), `RentalData`, `DataStore` (file) |
| Wiring | `RentalSystem` (composition root), `Clock` (System/Manual), `DemoData` |
| UI | `app/ConsoleIO`, `app/ConsoleApp*.cpp`, `app/main.cpp` |

## 7. Milestones (git history)

The project was built incrementally; every commit compiles and passes its tests.
Run `git log --oneline` to see them and `git checkout <hash>` to inspect one.

1. Project skeleton and domain objects
2. Vehicle and customer management (repositories, services, first menus)
3. + 4. Dates, availability, bookings, dynamic pricing and quotes - combined
   because in this design a booking can only be created *from* a quote, so the
   booking API depends on pricing from its first line
5. Pickup, return, invoices, maintenance
6. Persistence, demo data, reports
7. Tests, documentation, demonstration preparation
