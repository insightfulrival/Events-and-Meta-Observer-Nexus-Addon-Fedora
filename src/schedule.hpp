#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace events {

// UTC seconds from midnight. The generated data is sorted and unique.
struct Event {
    std::string name, colloquial, category, location, difficulty, waypoint, wiki, icon;
    int duration_minutes = -1;
    int alert_minutes = -1;
    std::vector<int> starts;
};

constexpr std::int64_t seconds_per_day = 86400;
constexpr std::int64_t never_alerted = std::numeric_limits<std::int64_t>::min();

inline std::string event_key(const Event& event) {
    return event.name + '\x1f' + event.category;
}

// Inclusive at the exact starting second, matching the original comparison.
// UTC arithmetic avoids daylight-saving transitions changing recurrence periods.
inline std::int64_t next_start(const Event& event, std::int64_t now) {
    if (event.starts.empty()) throw std::invalid_argument("Event has no start times");
    auto day = now / seconds_per_day;
    auto second = now % seconds_per_day;
    if (second < 0) { --day; second += seconds_per_day; }
    const auto next = std::lower_bound(event.starts.begin(), event.starts.end(), second);
    return next == event.starts.end()
        ? (day + 1) * seconds_per_day + event.starts.front()
        : day * seconds_per_day + *next;
}

// The marker belongs to one event and survives watch/notification toggles.
// Pausing notifications never consumes a reminder, and resuming cannot repeat
// an already delivered reminder for the same occurrence. No catch-up after start.
inline bool should_alert(const Event& event, std::int64_t now, bool watched,
                         bool notifications_enabled, std::int64_t& last_alerted_occurrence) {
    if (!watched || !notifications_enabled || event.alert_minutes <= 0) return false;
    const auto upcoming = next_start(event, now);
    const auto remaining = upcoming - now;
    if (remaining <= 0 || remaining >= std::int64_t(event.alert_minutes) * 60
        || upcoming <= last_alerted_occurrence) return false;
    last_alerted_occurrence = upcoming;
    return true;
}

} // namespace events
