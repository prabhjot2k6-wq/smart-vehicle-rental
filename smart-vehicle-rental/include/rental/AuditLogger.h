#pragma once
// AuditLogger: a concrete OBSERVER that keeps a permanent audit trail.
//   * appends one line per event to a text file (data/audit_log.txt)
//   * remembers the most recent entries in memory so the website's
//     "Recent activity" panel can show them
#include "rental/BookingObserver.h"
#include "rental/RentalData.h"

#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace rental {

struct ActivityEntry {
    std::string symbol; // small icon for the UI
    std::string title;  // "Booking confirmed"
    std::string text;   // "B-0007 · Aarav Sharma · Maruti Swift"
    std::string when;   // "2026-11-12 14:03"
};

class AuditLogger final : public BookingObserver {
public:
    // `data` is only read (to turn IDs into names). An empty path means
    // "keep entries in memory only" (used by tests).
    AuditLogger(const RentalData& data, std::filesystem::path logFile, std::size_t keep = 50);

    void onBookingEvent(const BookingEvent& event) override;

    // Newest first.
    std::vector<ActivityEntry> recent(std::size_t max) const;
    std::size_t count() const;

    static std::string formatTime(std::int64_t epochSeconds);

private:
    const RentalData& data_;
    std::filesystem::path file_;
    std::size_t keep_;
    mutable std::mutex mutex_; // the web server calls us from several threads
    std::deque<ActivityEntry> entries_; // oldest first
};

} // namespace rental
