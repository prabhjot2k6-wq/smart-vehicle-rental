// Saving and loading: round trips, escaping, ID preservation and corruption.

#include "TestFramework.h"
#include "TestHelpers.h"

#include "rental/DataStore.h"
#include "rental/DemoData.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace rental;
using namespace testhelp;
namespace fs = std::filesystem;

namespace {

// A fresh, empty folder that is deleted again at the end of the test (RAII).
struct TempDir {
    fs::path path;
    TempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("svrms_test_" + std::to_string(stamp));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

void writeFile(const fs::path& p, const std::string& content) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << content;
}

// Builds the demo data set as of 2026-11-09.
struct DemoSystem {
    ManualClock clock{D("2026-11-09")};
    RentalSystem sys{RentalData{}, clock};
    DemoSystem() { populateDemoData(sys, clock, D("2026-11-09")); }
};

std::string replaceFirst(std::string text, const std::string& from, const std::string& to) {
    const auto pos = text.find(from);
    if (pos != std::string::npos) text.replace(pos, from.size(), to);
    return text;
}

} // namespace

TEST_CASE("persistence: escaping handles tabs, newlines and backslashes") {
    const std::string nasty = "Scratch\ton door\nrear, \"left\" \\ 50% done\r";
    const std::string escaped = DataStore::escapeField(nasty);
    CHECK(escaped.find('\t') == std::string::npos);
    CHECK(escaped.find('\n') == std::string::npos);
    CHECK_EQ(DataStore::unescapeField(escaped), nasty);
    const auto fields = DataStore::splitRecord("A\t\tB\\tC\t");
    CHECK_EQ(fields.size(), std::size_t{4});
    CHECK_EQ(fields[1], std::string(""));
    CHECK_EQ(fields[2], std::string("B\tC"));
    CHECK_THROWS_AS(DataStore::unescapeField("bad\\q"), PersistenceError);
    CHECK_THROWS_AS(DataStore::unescapeField("bad\\"), PersistenceError);
}

TEST_CASE("persistence: save and reload keeps every record, type and price") {
    TempDir dir;
    DemoSystem demo;
    // Add awkward text to prove it survives.
    const std::string notes = "Dent\tnear handle\nCustomer said: \"it was already there\" \\ check photos";
    const std::string b = demo.sys.rentals()
                              .confirmQuote(demo.sys.quotes()
                                                .createQuote("C-0002", "V-0012", R("2026-11-20", "2026-11-22"))
                                                .id)
                              .id();
    demo.clock.setToday(D("2026-11-20"));
    demo.sys.rentals().pickup(b, InspectionReport(D("2026-11-20"), 16000, 90, notes));

    const DataStore store(dir.path / "data" / "rental_data.txt");
    store.save(demo.sys.data());
    const RentalData loaded = store.load();
    const RentalData& orig = demo.sys.data();

    CHECK_EQ(loaded.models.size(), orig.models.size());
    CHECK_EQ(loaded.vehicles.size(), orig.vehicles.size());
    CHECK_EQ(loaded.customers.size(), orig.customers.size());
    CHECK_EQ(loaded.bookings.size(), orig.bookings.size());
    CHECK_EQ(loaded.invoices.size(), orig.invoices.size());
    CHECK_EQ(loaded.maintenance.size(), orig.maintenance.size());

    // Derived types are rebuilt correctly.
    for (const Vehicle* v : orig.vehicles.all()) {
        const Vehicle& w = loaded.vehicles.get(v->id());
        CHECK(w.category() == v->category());
        CHECK_EQ(w.specValue(), v->specValue());
        CHECK_EQ(w.securityDeposit(), v->securityDeposit());
        CHECK_EQ(w.status(), v->status());
    }
    for (const Booking* bk : orig.bookings.all()) {
        const Booking& l = loaded.bookings.get(bk->id());
        CHECK_EQ(l.status(), bk->status());
        CHECK_EQ(l.agreedRentalTotal(), bk->agreedRentalTotal());
        CHECK(l.period() == bk->period());
        CHECK_EQ(l.priceParts().front().days().size(), bk->priceParts().front().days().size());
    }
    for (const Invoice* inv : orig.invoices.all()) {
        CHECK_EQ(loaded.invoices.get(inv->id()).totalCharges(), inv->totalCharges());
    }
    CHECK_EQ(loaded.bookings.get(b).pickupReport()->notes(), notes);
    CHECK_EQ(loaded.pricingRules.holidays().size(), orig.pricingRules.holidays().size());

    // The strongest check: saving the loaded data gives a byte-identical file.
    CHECK(DataStore::serialize(loaded) == DataStore::serialize(orig));
}

TEST_CASE("persistence: IDs continue after a restart without collisions") {
    TempDir dir;
    const DataStore store(dir.path / "rental_data.txt");
    std::string lastBooking;
    {
        DemoSystem demo;
        lastBooking = demo.sys.data().bookings.all().back()->id();
        store.save(demo.sys.data());
    }
    ManualClock clock(D("2026-11-09"));
    RentalSystem restarted(store.load(), clock);
    const std::string newVehicle =
        restarted.fleet().addVehicle("M-0001", "DM01SW0099", FuelType::Petrol, 100, 5).id();
    CHECK(restarted.data().vehicles.size() == 13);
    CHECK_EQ(newVehicle, std::string("V-0013"));
    const std::string q = restarted.quotes().createQuote("C-0001", newVehicle, R("2026-11-12", "2026-11-13")).id;
    const std::string newBooking = restarted.rentals().confirmQuote(q).id();
    CHECK(newBooking != lastBooking);
    CHECK_EQ(restarted.data().bookings.all().back()->id(), newBooking);
    CHECK_EQ(restarted.data().ids.next("INV"), std::string("INV-0003")); // two invoices existed
}

TEST_CASE("persistence: safe save keeps a backup and leaves no temp file") {
    TempDir dir;
    Fleet f;
    const DataStore store(dir.path / "rental_data.txt");
    store.save(f.sys.data());
    CHECK(store.exists());
    CHECK(!fs::exists(store.backupPath()));
    const std::string first = readFile(store.path());
    f.sys.fleet().addVehicle(f.swift, "MH12SW0099", FuelType::Petrol, 0, 5);
    store.save(f.sys.data());
    CHECK(fs::exists(store.backupPath()));
    CHECK(!fs::exists(store.tempPath()));
    CHECK_EQ(readFile(store.backupPath()), first);
    CHECK(readFile(store.path()) != first);
}

TEST_CASE("persistence: corrupted files are reported and never overwritten") {
    TempDir dir;
    DemoSystem demo;
    const std::string good = DataStore::serialize(demo.sys.data());
    const fs::path file = dir.path / "rental_data.txt";
    const DataStore store(file);

    const auto expectRejected = [&](const std::string& content) {
        writeFile(file, content);
        CHECK_THROWS_AS(store.load(), PersistenceError);
        CHECK_EQ(readFile(file), content); // untouched
    };
    expectRejected("");                                                       // empty
    expectRejected("hello world\n");                                          // bad header
    expectRejected(replaceFirst(good, "SVRMS-DATA\t1", "SVRMS-DATA\t9"));     // newer version
    expectRejected(good.substr(0, good.size() / 2));                          // truncated: no END
    expectRejected(replaceFirst(good, "VEHICLE\tV-0001\tCar", "VEHICLE\tV-0001\tTruck")); // bad enum
    expectRejected(replaceFirst(good, "MODEL\tM-0001", "MODEL\tM-0099"));    // vehicles lose their model
    expectRejected(replaceFirst(good, "\tDM01SW0002\t", "\tDM01SW0001\t"));  // duplicate registration
    expectRejected(replaceFirst(good, "BOOKING\tB-0001\tC-0002", "BOOKING\tB-0001\tC-0099")); // missing customer
    expectRejected(replaceFirst(good, "INSPECT\tB-0001\tPICKUP\t", "INSPECT\tB-0001\tPICKED\t"));
    // Tampering with a stored total is detected by the day-line cross-check.
    const auto priceLine = good.find("PRICE\tB-0001");
    const auto lineEnd = good.find('\n', priceLine);
    std::string tampered = good;
    tampered.replace(priceLine, lineEnd - priceLine, good.substr(priceLine, lineEnd - priceLine) + "9");
    expectRejected(tampered);
    // Extra record count mismatch.
    expectRejected(replaceFirst(good, "END\t", "END\t1"));

    writeFile(file, good);
    CHECK_EQ(store.load().bookings.size(), demo.sys.data().bookings.size());
}

TEST_CASE("demo data: builds a realistic history through the real services") {
    DemoSystem demo;
    const RentalData& d = demo.sys.data();
    CHECK_EQ(d.vehicles.size(), std::size_t{12});
    CHECK_EQ(d.invoices.size(), std::size_t{2});
    CHECK_EQ(d.vehicles.get("V-0006").status(), VehicleStatus::UnderMaintenance);
    const auto active = demo.sys.reports().activeRentals(D("2026-11-09"));
    CHECK_EQ(active.size(), std::size_t{1});
    // Next week 3 of 4 Swifts are booked: the 4th is quoted at the 1.15x tier.
    const auto p = demo.sys.quotes().priceFor(d.vehicles.get("V-0004"), R("2026-11-16", "2026-11-17"));
    CHECK_EQ(p.days()[0].demandFactorBp, BasisPoints{11500});
    const RevenueSummary rev = demo.sys.reports().revenue(std::nullopt);
    CHECK_EQ(rev.invoiceCount, 2);
    CHECK_EQ(rev.total, d.invoices.get("INV-0001").totalCharges() + d.invoices.get("INV-0002").totalCharges());
    CHECK(!demo.sys.maintenance().serviceAlerts().empty()); // the van is near its service
}
