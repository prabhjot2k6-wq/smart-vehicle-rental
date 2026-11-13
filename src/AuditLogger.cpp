#include "rental/AuditLogger.h"

#include <ctime>
#include <fstream>

namespace rental {

namespace {

const char* symbolFor(BookingEventKind k) {
    switch (k) {
    case BookingEventKind::Confirmed: return "✓";
    case BookingEventKind::Extended: return "↻";
    case BookingEventKind::PickedUp: return "→";
    case BookingEventKind::Returned: return "←";
    case BookingEventKind::Cancelled: return "✕";
    }
    return "•";
}

const char* titleFor(BookingEventKind k) {
    switch (k) {
    case BookingEventKind::Confirmed: return "Booking confirmed";
    case BookingEventKind::Extended: return "Booking extended";
    case BookingEventKind::PickedUp: return "Vehicle picked up";
    case BookingEventKind::Returned: return "Vehicle returned";
    case BookingEventKind::Cancelled: return "Booking cancelled";
    }
    return "Booking event";
}

} // namespace

AuditLogger::AuditLogger(const RentalData& data, std::filesystem::path logFile, std::size_t keep)
    : data_(data), file_(std::move(logFile)), keep_(keep) {}

std::string AuditLogger::formatTime(std::int64_t epochSeconds) {
    const std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &local);
    return buf;
}

void AuditLogger::onBookingEvent(const BookingEvent& e) {
    std::string who = e.customerId;
    if (const Customer* c = data_.customers.find(e.customerId)) who = c->name();
    std::string what = e.vehicleId;
    if (const Vehicle* v = data_.vehicles.find(e.vehicleId)) {
        if (const VehicleModel* m = data_.models.find(v->modelId())) what = m->displayName();
    }

    ActivityEntry entry;
    entry.symbol = symbolFor(e.kind);
    entry.title = titleFor(e.kind);
    entry.text = e.bookingId + " · " + who + " · " + what + (e.detail.empty() ? "" : " · " + e.detail);
    entry.when = formatTime(e.epochSeconds);

    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(entry);
    while (entries_.size() > keep_) entries_.pop_front();

    if (!file_.empty()) {
        std::ofstream out(file_, std::ios::app);
        if (out) out << entry.when << '\t' << toString(e.kind) << '\t' << entry.text << '\n';
    }
}

std::size_t AuditLogger::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

std::vector<ActivityEntry> AuditLogger::recent(std::size_t max) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ActivityEntry> out;
    for (auto it = entries_.rbegin(); it != entries_.rend() && out.size() < max; ++it) out.push_back(*it);
    return out;
}

} // namespace rental
