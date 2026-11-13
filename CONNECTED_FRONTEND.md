# Smart Vehicle Rental — browser + C++ backend

## Requirements
- CMake 3.16+
- C++17 compiler (GCC 9+ / recent MinGW-w64, or recent MSVC)
- Boost headers (Beast, Asio and Property Tree; header-only use in this project)

On Windows with MSYS2 UCRT64, install a recent compiler/CMake/Boost toolchain, e.g. the `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-cmake`, and `mingw-w64-ucrt-x86_64-boost` packages. Use the matching UCRT64 terminal so CMake finds the same compiler and Boost headers. The old MinGW GCC 6.3 toolchain is too old for this project.

## Start on Windows
1. Extract the ZIP.
2. Open the project root in the MSYS2 UCRT64 terminal (or configure CMake with your installed MinGW-w64 toolchain).
3. Run `run-api.bat` from Command Prompt after CMake, compiler and Boost are on PATH / discoverable by CMake.
4. Keep the terminal running; it prints `http://127.0.0.1:8765`.
5. Open `frontend/index.html` in Chrome.

If CMake cannot find Boost, set `Boost_ROOT` to the Boost installation prefix or install the Boost headers for the compiler you're using.

## What is connected
- `GET /api/state`: reads vehicles, customers, bookings and maintenance from the C++ `RentalSystem`.
- `POST /api/customers`: registers customers using `CustomerService`.
- `POST /api/vehicles`: creates a model and vehicle using `FleetService`.
- `POST /api/bookings`: obtains a real quote and confirms it using `QuoteService` and `RentalService`.
- `POST /api/bookings/{id}/pickup`: records pickup through `RentalService`.
- `POST /api/bookings/{id}/return`: completes the return and generates the invoice through `RentalService`.
- `POST /api/bookings/{id}/cancel`: cancels through `RentalService`.
- `POST /api/maintenance`: opens a maintenance task through `MaintenanceService`.

All successful writes are saved through `DataStore` to `data/rental_data.txt`. The API binds to loopback (`127.0.0.1`) only; it is intended for a local college demo, not public deployment.

The frontend's monthly revenue chart remains illustrative, and some dashboard activity/report copy is presentation-only. The fleet, customer, booking, price quote, pickup/return, invoice and maintenance operations listed above use the C++ backend.

## OOP extensions (Observer, customer kinds, Factory)
- Customers now have a **type** (Regular / Premium / Corporate). Choose it in "Add a customer". Premium gets 5% and Corporate 10% off every quote, and each type has its own limit of open bookings.
- Every booking step (confirm, extend, pickup, return, cancel) is announced to observers. The dashboard's **Recent activity** panel shows them, the server terminal prints a simulated SMS, and `data/audit_log.txt` keeps a permanent log.
- `POST /api/customers` accepts an optional `"type"` field. `GET /api/state` now returns each customer's `type`, `discount` and `limit`, and fills `activity`.
- Full explanation, diagrams and a demo script: `docs/OOP_EXTENSIONS.md`.
