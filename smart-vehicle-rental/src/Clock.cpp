#include "rental/Clock.h"

#include <ctime>

namespace rental {

std::int64_t SystemClock::nowEpochSeconds() const {
    return static_cast<std::int64_t>(std::time(nullptr));
}

Date SystemClock::today() const {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return Date::fromYmd(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

} // namespace rental
