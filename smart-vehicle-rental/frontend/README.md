# DriveFlow browser frontend

This frontend is connected to the C++ `RentalSystem` through the local HTTP API.
It is not a browser-only/localStorage demo anymore.

## Run it

1. Start the C++ API from the project root with `run-api.bat` (Windows), or build and run the `rental_api` CMake target.
2. Keep the API terminal open. It listens only on `127.0.0.1:8765`.
3. Open this folder's `index.html` in Chrome.

The frontend reads fleet, customer, booking and maintenance data from `/api/state`. Creating customers, vehicles, bookings and maintenance records calls the C++ services. Booking pickup and return also go through the C++ rental lifecycle. The C++ `DataStore` persists changes to `data/rental_data.txt`.

If Chrome says it cannot reach the backend, start `rental_api` first and reload the page.
