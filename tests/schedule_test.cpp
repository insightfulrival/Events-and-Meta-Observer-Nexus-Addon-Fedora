#include "../src/schedule.hpp"
#include "../generated/events.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace events;
    const auto catalog = bundled_events();
    assert(catalog.size() == 88);
    const auto find = [&](const std::string& name) -> const Event& {
        auto it = std::find_if(catalog.begin(), catalog.end(), [&](const Event& e) { return e.name == name; });
        assert(it != catalog.end()); return *it;
    };

    for (const auto& event : catalog) {
        assert(!event.starts.empty());
        assert(std::is_sorted(event.starts.begin(), event.starts.end()));
        assert(std::adjacent_find(event.starts.begin(), event.starts.end()) == event.starts.end());
        assert(event.starts.front() >= 0 && event.starts.back() < seconds_per_day);
        for (int start : event.starts) {
            assert(next_start(event, start) == start);
            assert(next_start(event, start - 1) == start);
            assert(next_start(event, start + 1) > start);
        }
    }

    for (const auto& event : catalog)
        for (int start : event.starts) {
            assert(previous_start(event, start) == start);
            assert(previous_start(event, start + 1) == start);
            assert(previous_start(event, start - 1) < start);
        }

    const auto& reset = find("Server Reset");
    assert(reset.starts == std::vector<int>{0});
    assert(next_start(reset, 0) == 0); // Original zero/default ambiguity fixed.
    assert(next_start(reset, 1) == 86400);
    assert(next_start(reset, 86399) == 86400);
    assert(next_start(reset, 86400) == 86400);
    assert(next_start(reset, -1) == 0);
    assert(next_start(reset, -86401) == -86400);

    const auto& anomaly = find("Ley-Line Anomaly (Timberline Falls)");
    assert((anomaly.starts == std::vector<int>{1200, 22800, 44400, 66000}));
    assert(next_start(anomaly, 86399) == 87600);
    const auto& covington = find("Admiral Taidha Covington");
    assert(covington.starts.size() == 8); // Three-hour repeat.
    assert(covington.starts.front() == 0 && covington.starts.back() == 75600);

    // Three bosses combine separate irregular rows; Jade Maw combines two
    // independent periodic phases. Neither can be replaced by one interval.
    assert((find("Evolved Jungle Wurm").starts == std::vector<int>{3600, 14400, 28800, 45000, 61200, 72000}));
    assert((find("Karka Queen").starts == std::vector<int>{7200, 21600, 37800, 54000, 64800, 82800}));
    assert((find("Tequatl the Sunless").starts == std::vector<int>{0, 10800, 25200, 41400, 57600, 68400}));
    const auto& jade = find("Jade Maw");
    assert(jade.starts.size() == 24);
    assert(next_start(jade, 301) == 2700);
    assert(next_start(jade, 2701) == 7500);

    Event reminder;
    reminder.starts = {3600, 7200}; reminder.alert_minutes = 10;
    auto last = never_alerted;
    assert(!should_alert(reminder, 2999, true, true, last));
    assert(!should_alert(reminder, 3000, true, true, last)); // Strict lead boundary.
    assert(!should_alert(reminder, 3001, false, true, last));
    assert(!should_alert(reminder, 3001, true, false, last));
    assert(last == never_alerted); // Muting/unwatching cannot consume an alert.
    assert(should_alert(reminder, 3001, true, true, last));
    assert(last == 3600);
    assert(!should_alert(reminder, 3010, true, true, last));
    assert(!should_alert(reminder, 3020, false, true, last));
    assert(!should_alert(reminder, 3030, true, true, last)); // No toggle spam.
    assert(!should_alert(reminder, 3002, true, true, last)); // Clock moves backwards.
    assert(!should_alert(reminder, 3600, true, true, last)); // Already started.
    assert(!should_alert(reminder, 3601, true, true, last)); // No missed-event catch-up.
    assert(should_alert(reminder, 6601, true, true, last)); // New occurrence.
    assert(last == 7200);
    assert(should_alert(reminder, 86400 + 3001, true, true, last)); // Next UTC day.
    reminder.alert_minutes = -1;
    last = never_alerted;
    assert(!should_alert(reminder, 3500, true, true, last));
    reminder.alert_minutes = 0;
    assert(!should_alert(reminder, 3500, true, true, last));

    Event running;
    running.starts = {3600, 7200}; running.duration_minutes = 15;
    assert(previous_start(running, 3599) == 7200 - 86400); // Previous UTC day.
    assert(!in_progress(running, 3599));
    assert(in_progress(running, 3600)); // Inclusive at the starting second.
    assert(in_progress(running, 3600 + 899));
    assert(!in_progress(running, 3600 + 900)); // Ends after its duration.
    assert(in_progress(running, 86400 + 7200 + 60));
    running.duration_minutes = -1;
    assert(!in_progress(running, 3600)); // Unknown duration.
    assert(!in_progress(reset, 0)); // Zero duration.

    // Each day-night phase ends exactly when the next one begins, in Tyria and Cantha.
    for (const char* suffix : {"", " - Cantha"}) {
        const char* phases[] = {"Dawn", "Day", "Dusk", "Night"};
        for (int i = 0; i < 4; ++i) {
            const auto& phase = find(std::string(phases[i]) + suffix);
            const auto& following = find(std::string(phases[(i + 1) % 4]) + suffix);
            const std::int64_t end = phase.starts.front() + phase.duration_minutes * 60;
            assert(next_start(following, end) == end);
        }
    }

    // A Berlin DST transition never moves UTC game schedules or repeat intervals.
    // 2026-10-25 00:00 UTC and +24h: reset remains exactly one UTC day apart.
    constexpr std::int64_t dst_day = 1792886400;
    assert(next_start(reset, dst_day + 1) - next_start(reset, dst_day) == 86400);
    std::cout << "Schedule tests passed: 88 events, UTC rollover, DST independence, running events, and alert deduplication.\n";
}
