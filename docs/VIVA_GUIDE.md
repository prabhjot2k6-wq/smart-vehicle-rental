# Viva guide

## 1. Five-minute demonstration script

Preparation (before the viva): build, delete any old `data` folder, and keep a
second terminal open in the project folder. In the steps below **T** means
today's date; the demo data is generated relative to the day you seed it.

| Time | Do this | Point out |
|---|---|---|
| 0:00 | `ctest --test-dir build --output-on-failure` | 49 automated tests: every pricing threshold, the cap, rounding, leap years, overlaps, expired quotes, return charges, save/reload, corrupted files. |
| 0:30 | `./build/rental_app --seed-demo` -> **7 Reports -> 1 Fleet availability** (Enter for today) | 12 vehicles of 6 models. V-0001 *On rent*, V-0008 *Reserved*, V-0006 *Under maintenance (blocked)*. "Rented" is derived from bookings, not stored. |
| 1:00 | **0 -> 3 Quotes -> 2 Create price quote**: customer `C-0002`, vehicle `V-0004`, pickup T+7, return T+10 | Three of the four Swifts are already booked that week -> *3/4 booked (75%) -> tier >= 70% -> 1.15x*. Each day shows demand, calendar factor and reason. Answer **n** to keep it pending. |
| 1:45 | **4 List pending quotes** | Countdown to expiry (15 min). Explain: expired quotes are rejected at confirmation; a valid quote is honoured even if prices rise. Then **3 Confirm a pending quote** -> `Q-1` -> **y**. |
| 2:15 | **2 Create price quote**: `C-0004`, `V-0002`, T+11 -> T+14 | Rejected: Meera's licence expires at T+12, before the return date. |
| 2:30 | **2 Create price quote**: `C-0001`, `V-0006`, any future dates | Rejected: under maintenance = **block**. Then **5 Pricing settings** to show the rules, holidays (T+5 is a demo holiday) and the Dynamic/Standard strategy switch (polymorphism). |
| 3:00 | **0 -> 5 Pickup and return -> 1 Collect**: `B-0004`, Enter for date/odometer/fuel | Deposit INR 5,000 from `Car::securityDeposit()` - a virtual call. |
| 3:20 | **2 Return vehicle**: `B-0003`, odometer 10750, fuel 60, add damage "Dented rear door" 2000 | Draft invoice preview first (nothing saved), then finalize. Fuel 40 points x INR 60; damage entered manually; deposit shown separately and not counted as revenue. |
| 4:00 | **0 -> 6 Maintenance -> 4 Service warnings** | V-0012 is 300 km from its service: a **warning**, still bookable - contrast with V-0006's block. |
| 4:20 | **0 -> 7 Reports -> 4 Revenue** (n for all time) | Only finalized invoices; rental vs additional charges. |
| 4:40 | **0 -> 8 Save and exit**, run `./build/rental_app` again, **4 -> 1 List bookings** | Same bookings and IDs after restart. Open `data/rental_data.txt`: versioned header, one record per line, `END` count; `.bak` holds the previous save. Running `--seed-demo` again is refused (no duplicate demo data). |

If anything is asked about the code, open `src/PricingStrategy.cpp`
(`DynamicPricingStrategy::price`) and `include/rental/Vehicle.h`.

## 2. Likely viva questions

**Q1. Why is `Vehicle` abstract?**
There is no such thing as a "generic vehicle" in the fleet; every vehicle is a
car, bike or van. Pure virtual functions (`category()`, `securityDeposit()`,
`serviceIntervalKm()`, `specLabel()`...) force every subclass to define them and
stop anyone creating a plain `Vehicle`.

**Q2. Why not a `Swift` class and a `Creta` class?**
Brands differ only in *data* (name, rate), not *behaviour*. They are
`VehicleModel` objects, so adding a model needs no new code. Car/Bike/Van do
differ in behaviour (deposit, service interval, extra attribute), so they are
classes.

**Q3. Where is runtime polymorphism?**
`QuoteService` calls `strategy_->price(context)` on a `unique_ptr<PricingStrategy>`:
Dynamic or Standard is chosen at run time. Pickup calls `vehicle.securityDeposit()`
through a `Vehicle&`. `Clock::today()` is `SystemClock` in the app and
`ManualClock` in tests. `makeVehicle()` rebuilds the right subclass when loading.

**Q4. Why does `Vehicle` have a virtual destructor and deleted copy?**
Vehicles are deleted through `unique_ptr<Vehicle>`; without a virtual destructor
the `Car` part would not be destroyed properly (undefined behaviour). Copying via
a base reference would slice off the derived part, so copying is disabled.

**Q5. Why money in paise instead of `double`?**
`double` cannot store 0.1 or 1.15 exactly, so totals drift by fractions of a paisa
and comparisons fail. Integers are exact; rounding happens once, at a defined
point, with a stated rule (half away from zero).

**Q6. How are percentages and multipliers stored?**
As basis points: 1.15x = 11500, 10% = 1000. `a x b / 10000` with integer rounding.
Thresholds compare `booked x 10000 >= threshold x capacity`, so 7/10 is exactly 70%.

**Q7. Why is the return date not billed?**
Periods are half-open `[pickup, return)`. Day counts are simply `return - pickup`,
and a car returned on the 15th can be picked up by the next customer on the 15th
without a "fake" overlap.

**Q8. How do you prevent double booking?**
`AvailabilityService::conflictingBookings` checks `a.start < b.end && b.start < a.end`
against every Confirmed/Active booking of that vehicle. It runs at quote time and
again at confirmation, extension and pickup.

**Q9. What exactly is "utilization"?**
Per day, per model: Confirmed + Active bookings covering that day on rentable
vehicles, divided by the number of Active vehicles of the model. Cancelled
bookings, vehicles under maintenance or inactive, and the customer's own
proposed booking are excluded. Zero capacity gives a neutral 1.00x.

**Q10. What if prices change after a booking is confirmed?**
Nothing happens to it: the booking stores its own `PriceBreakdown` (a copy).
The test "confirmed price preserved after rate and rule changes" proves it.

**Q11. What if demand rises while a quote is pending?**
An unexpired quote is honoured at the quoted price (if the vehicle is still
free). After 15 minutes it is rejected and the customer must re-quote.

**Q12. How is an extension checked?**
The extra days `[old return, new return)` are checked for clashes while
ignoring the booking itself, the licence must cover the new return date, and the
extra days are priced separately and shown before confirming.

**Q13. Warning vs block in maintenance?**
A warning (within 500 km of the service threshold) is information only. A block
happens when a maintenance record is open: the vehicle becomes
`UnderMaintenance`, leaves the rentable capacity and cannot be quoted, booked or
collected.

**Q14. What is composition in your project?**
A `Booking` owns its price breakdowns and inspection reports; an `Invoice` owns
its lines; a `Customer` owns its `DrivingLicence`; `RentalSystem` owns the data
and services. The parts are created and destroyed with the whole.

**Q15. Why so many service classes instead of one manager?**
Single responsibility: each class has one reason to change. Pricing changes touch
`PricingStrategy`/`QuoteService`; charge changes touch `ChargeCalculator`. It also
makes each part testable on its own.

**Q16. Why is `RentalSystem` not copyable?**
Its services hold references to its `RentalData`. A copy would have services
pointing at the *original* data. Deleting the copy operations makes that mistake
impossible.

**Q17. How do you save without corrupting data?**
Write to a `.tmp` file, read it back to prove it loads, rename the old file to
`.bak`, then rename `.tmp` into place. On load every record and reference is
validated; a bad file is reported with its line number and never overwritten.

**Q18. How do IDs survive restarts?**
`IdGenerator` counters are saved. On load each counter is also raised to the
largest existing ID, so even a damaged counter cannot cause a collision.

**Q19. How do you rebuild a `Car` from text?**
The `VEHICLE` record stores the category; `makeVehicle(category, data, spec)` is a
factory that returns the right `unique_ptr<Car/Bike/Van>`.

**Q20. How did you test time-dependent features like expiry?**
Services take a `Clock&`. Tests pass a `ManualClock` and call
`advanceSeconds(15 * 60)` - no waiting, fully repeatable.

**Q21. Where are exceptions used and why?**
Every broken rule throws a `RentalError` subclass with a clear message. The menu
catches it in one place (`ConsoleApp::runAction`) and shows it, so the program
never crashes on bad input and the rules stay out of the UI.

**Q22. How do you handle invalid numeric input?**
Input is always read as a whole line and parsed manually; on error the prompt
repeats. `std::cin` is never left in a failed state. Typing `cancel` backs out.

## 3. Known limitations

* Whole-day billing only; no pickup/return times, no hourly charges.
* Single process, no file locking, no user accounts or roles.
* No `fsync` after writing; on Windows the backup/replace is two separate renames
  (the `.bak` covers the gap).
* Pending quotes are in memory only; the chosen pricing strategy is not saved.
* Maintenance blocks start immediately; future maintenance windows cannot be scheduled.
* Deposits are always refunded in full; charges are settled separately (simulated).
* No cancellation fee and no refund for early return.
* Extension discounts are based on the extension's own length, not the whole rental.
* Holidays are plain dates (no names); weekends are fixed to Saturday/Sunday.
* Customers and vehicles are never deleted (deactivate only) to keep history.
* Table alignment counts bytes, so non-ASCII names may misalign columns.
* Searching all bookings is linear; fine for a course-sized fleet.

## 4. Sensible future improvements

1. Scheduled maintenance windows that block only their dates.
2. Hourly billing with pickup/return times and grace periods.
3. Deposit deductions and real payment status per invoice.
4. Cancellation policy (fees by notice period) and early-return rules.
5. Early-booking and loyalty discounts as new `PricingStrategy` classes or
   decorators - added only now that the core rules are tested.
6. Editing every pricing/charge rule from the menu; CSV export of reports.
7. SQLite or another database, with transactions instead of whole-file saves.
8. Staff logins with roles (counter staff vs manager).
9. A GUI or web front end reusing `src/` unchanged.
