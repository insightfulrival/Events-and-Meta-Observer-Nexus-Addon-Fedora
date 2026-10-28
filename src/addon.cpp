#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Nexus.h"
#include "imgui.h"
#include "schedule.hpp"
#include "events.hpp"
#include "icon.hpp"
#include "icons.hpp"
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <string>
#include <vector>

static_assert(sizeof(void*) == 8, "Nexus requires a Windows x64 addon");
static_assert(sizeof(AddonDefinition_t) == 72);
static_assert(offsetof(AddonDefinition_t, Load) == 40);
static_assert(offsetof(AddonAPI_t, GUI_Register) == 32);
static_assert(offsetof(AddonAPI_t, Paths_GetAddonDirectory) == 96);
static_assert(IMGUI_VERSION_NUM == 18000, "Use the ImGui shipped by Nexus");

namespace {
constexpr const char* window_name = "Events and Metas Observer##nexus.events";
constexpr const char* bind_id = "EVOBS_TOGGLE_WINDOW";
constexpr const char* shortcut_id = "EVOBS_SHORTCUT";
constexpr const char* texture_id = "EVOBS_ICON";
AddonAPI_t* api = nullptr;
std::atomic<bool> toggle_requested{false};
bool visible = false;
bool notifications_enabled = true;
bool chime_enabled = true;
bool move_notifications = false;
bool use_utc = false;
bool next_up = true;
bool watched_only = false;
bool scroll_to_top_requested = false;
float notification_x = 180, notification_y = 60;
char search[160] = {};
std::string category;
std::filesystem::path settings_path;
std::vector<events::Event> all_events;
std::vector<unsigned char> watched;
std::vector<std::int64_t> last_alerted;
std::vector<std::string> texture_ids;
struct Notice { std::size_t event; double expires; bool preview; std::int64_t occurrence; };
std::vector<Notice> notices;
std::int64_t last_check = events::never_alerted;

void log(ELogLevel level, const char* message) {
    if (api && api->Log) api->Log(level, "Events Observer", message);
}
void feedback(const char* message) {
    if (api && api->GUI_SendAlert) api->GUI_SendAlert(message);
}
void save_settings() {
    try {
        auto tmp = settings_path;
        tmp += ".tmp";
        std::ofstream out(tmp, std::ios::trunc);
        out << "version 1\nnotifications " << notifications_enabled
            << "\nchime " << chime_enabled << "\nutc " << use_utc
            << "\nposition " << notification_x << ' ' << notification_y << '\n';
        for (std::size_t i = 0; i < all_events.size(); ++i)
            out << "watch " << std::quoted(all_events[i].name) << ' '
                << std::quoted(all_events[i].category) << ' ' << int(watched[i]) << '\n';
        out.flush();
        if (!out) throw std::runtime_error("Could not write settings");
        out.close();
        if (!MoveFileExW(tmp.c_str(), settings_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace settings");
    } catch (const std::exception& e) {
        log(LOGL_WARNING, e.what());
        feedback("Events Observer: settings could not be saved. Check nexus.log.");
    }
}
void load_settings() {
    std::ifstream in(settings_path);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "notifications") row >> notifications_enabled;
        else if (key == "chime") row >> chime_enabled;
        else if (key == "utc") row >> use_utc;
        else if (key == "position") {
            float x, y;
            if (row >> x >> y && std::isfinite(x) && std::isfinite(y)) {
                notification_x = std::clamp(x, 0.f, 16000.f);
                notification_y = std::clamp(y, 0.f, 16000.f);
            }
        } else if (key == "watch") {
            std::string name, group;
            int enabled;
            if (row >> std::quoted(name) >> std::quoted(group) >> enabled)
                for (std::size_t i = 0; i < all_events.size(); ++i)
                    if (all_events[i].name == name && all_events[i].category == group)
                        watched[i] = enabled != 0;
        }
    }
}
std::string lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
std::string time_label(std::int64_t timestamp) {
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm result{};
    if (use_utc) gmtime_s(&result, &t); else localtime_s(&result, &t);
    char buffer[32];
    std::strftime(buffer, sizeof buffer, "%H:%M", &result);
    return buffer;
}
std::string countdown(std::int64_t seconds) {
    auto minutes = (std::max<std::int64_t>(seconds, 0) + 59) / 60;
    if (minutes >= 60) return std::to_string(minutes / 60) + "h " + std::to_string(minutes % 60) + "m";
    return std::to_string(minutes) + "m";
}
void copy_waypoint(const std::string& waypoint) {
    if (waypoint.empty()) return;
    bool success = false;
    if (OpenClipboard(nullptr)) {
        auto memory = GlobalAlloc(GMEM_MOVEABLE, (waypoint.size() + 1) * sizeof(wchar_t));
        if (memory) {
            auto* text = static_cast<wchar_t*>(GlobalLock(memory));
            if (text) {
                for (std::size_t i = 0; i <= waypoint.size(); ++i) text[i] = static_cast<unsigned char>(waypoint[i]);
                GlobalUnlock(memory);
                if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory)) success = true;
            }
            if (!success) GlobalFree(memory);
        }
        CloseClipboard();
    }
    feedback(success ? "Waypoint copied. Paste it in game chat." : "Could not copy waypoint. Try again.");
}
void open_wiki(const std::string& url) {
    if (url.rfind("https://wiki.guildwars2.com/", 0) != 0) return;
    int count = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    if (count <= 0) return;
    std::wstring wide(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wide.data(), count);
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
        feedback("Could not open the wiki. Check Wine browser integration.");
}
Texture_t* event_texture(std::size_t index) {
    if (!api->Textures_Get) return nullptr;
    return api->Textures_Get(texture_ids[index].c_str());
}
void draw_event_icon(std::size_t index, float size) {
    auto* texture = event_texture(index);
    if (texture && texture->Resource) ImGui::Image(texture->Resource, ImVec2(size, size));
    else ImGui::Dummy(ImVec2(size, size));
}
void scan_alerts(std::int64_t now) {
    if (now == last_check) return;
    last_check = now;
    bool sound = false;
    for (std::size_t i = 0; i < all_events.size(); ++i) {
        if (events::should_alert(all_events[i], now, watched[i] != 0, notifications_enabled, last_alerted[i])) {
            notices.push_back({i, ImGui::GetTime() + 10.0, false, events::next_start(all_events[i], now)});
            sound = true;
        }
    }
    if (sound && chime_enabled) MessageBeep(MB_OK);
}
void draw_notices(std::int64_t now) {
    double tick = ImGui::GetTime();
    notices.erase(std::remove_if(notices.begin(), notices.end(), [tick](const Notice& n) { return n.expires <= tick; }), notices.end());
    if (notices.empty() && !move_notifications) return;
    const auto display = ImGui::GetIO().DisplaySize;
    float width = std::min(360.f, std::max(180.f, display.x));
    float height = std::min(std::max(120.f, display.y - 30.f), 500.f);
    notification_x = std::clamp(notification_x, 0.f, std::max(0.f, display.x - width));
    notification_y = std::clamp(notification_y, 0.f, std::max(0.f, display.y - 120.f));
    ImGui::SetNextWindowPos(ImVec2(notification_x, notification_y), move_notifications ? ImGuiCond_Appearing : ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 60), ImVec2(width, height));
    auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (!move_notifications) flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
    bool open = true;
    if (ImGui::Begin("Event notifications##evobs", move_notifications ? &open : nullptr, flags)) {
        if (move_notifications) {
            ImGui::TextWrapped("Drag this title bar to position notifications.");
            auto p = ImGui::GetWindowPos();
            notification_x = p.x; notification_y = p.y;
            if (ImGui::Button("Save position")) { move_notifications = false; save_settings(); }
        }
        for (std::size_t i = 0; i < notices.size();) {
            const auto& n = notices[i];
            const auto& e = all_events[n.event];
            ImGui::PushID(static_cast<int>(i));
            draw_event_icon(n.event, 40);
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 70);
            ImGui::TextUnformatted(e.name.c_str());
            auto message = n.preview ? std::string("Test notification") : (n.occurrence <= now ? std::string("Starting now") : "Starts in " + countdown(n.occurrence - now));
            ImGui::TextDisabled("%s", message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            bool dismiss = ImGui::IsItemClicked(1);
            if (ImGui::IsItemClicked(0)) { copy_waypoint(e.waypoint); dismiss = true; }
            if (!e.waypoint.empty() && ImGui::IsItemHovered()) ImGui::SetTooltip("Left click: copy waypoint\nRight click: dismiss");
            if (ImGui::SmallButton("Dismiss")) dismiss = true;
            ImGui::SameLine();
            bool dismiss_all = ImGui::SmallButton("Dismiss all");
            ImGui::Separator();
            ImGui::PopID();
            if (dismiss_all) { notices.clear(); break; }
            if (dismiss) notices.erase(notices.begin() + static_cast<std::ptrdiff_t>(i)); else ++i;
        }
    }
    ImGui::End();
    if (!open) { move_notifications = false; save_settings(); }
}
void draw_controls() {
    if (ImGui::Checkbox("Enable notifications", &notifications_enabled)) {
        if (!notifications_enabled) notices.clear();
        save_settings();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Sound", &chime_enabled)) save_settings();
    ImGui::SameLine();
    if (ImGui::Checkbox("UTC times", &use_utc)) save_settings();
    if (ImGui::Button(move_notifications ? "Finish positioning" : "Move notifications")) {
        move_notifications = !move_notifications;
        if (!move_notifications) save_settings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Test notification") && !all_events.empty()) {
        notices.push_back({0, ImGui::GetTime() + 10.0, true, 0});
        if (chime_enabled) MessageBeep(MB_OK);
    }
}
void draw_window(std::int64_t now) {
    if (!visible) return;
    ImGui::SetNextWindowSize(ImVec2(850, 620), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(window_name, &visible)) {
        draw_controls();
        ImGui::Separator();
        ImGui::SetNextItemWidth(240);
        ImGui::InputText("Search", search, sizeof search);
        ImGui::SameLine();
        ImGui::Checkbox("Watched only", &watched_only);
        ImGui::SameLine();
        ImGui::Checkbox("Sort by next start", &next_up);
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("Category", category.empty() ? "All events" : category.c_str())) {
            if (ImGui::Selectable("All events", category.empty())) category.clear();
            std::vector<std::string> groups;
            for (const auto& e : all_events)
                if (std::find(groups.begin(), groups.end(), e.category) == groups.end()) groups.push_back(e.category);
            std::sort(groups.begin(), groups.end());
            for (const auto& group : groups)
                if (ImGui::Selectable(group.c_str(), group == category)) category = group;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Watch all")) { std::fill(watched.begin(), watched.end(), 1); save_settings(); }
        ImGui::SameLine();
        if (ImGui::Button("Watch none")) { std::fill(watched.begin(), watched.end(), 0); save_settings(); }
        ImGui::TextDisabled("%zu events | %s | Ctrl+Alt+E toggles this window", all_events.size(), use_utc ? "UTC" : "Local time");
        auto query = lower(search);
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < all_events.size(); ++i) {
            const auto& e = all_events[i];
            if ((!category.empty() && category != e.category) || (watched_only && !watched[i])) continue;
            if (!query.empty() && lower(e.name + " " + e.colloquial + " " + e.location).find(query) == std::string::npos) continue;
            order.push_back(i);
        }
        std::sort(order.begin(), order.end(), [now](std::size_t a, std::size_t b) {
            if (next_up) {
                auto ta = events::next_start(all_events[a], now), tb = events::next_start(all_events[b], now);
                if (ta != tb) return ta < tb;
            }
            return all_events[a].name < all_events[b].name;
        });
        if (ImGui::BeginChild("Event list", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true)) {
            if (scroll_to_top_requested) {
                ImGui::SetScrollY(0.0f);
                scroll_to_top_requested = false;
            }
            for (auto index : order) {
                const auto& e = all_events[index];
                ImGui::PushID(static_cast<int>(index));
                bool enabled = watched[index] != 0;
                if (ImGui::Checkbox("##watch", &enabled)) { watched[index] = enabled; save_settings(); }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Notify me before this event starts");
                ImGui::SameLine(); draw_event_icon(index, 32); ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextUnformatted(e.name.c_str());
                auto next = events::next_start(e, now);
                ImGui::TextDisabled("%s  |  in %s  |  %s", time_label(next).c_str(), countdown(next - now).c_str(), e.location.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Category: %s", e.category.c_str());
                    if (!e.difficulty.empty()) ImGui::Text("Difficulty: %s", e.difficulty.c_str());
                    if (e.duration_minutes >= 0) ImGui::Text("Duration: %d minutes", e.duration_minutes);
                    if (e.alert_minutes >= 0) ImGui::Text("Reminder: %d minutes before start", e.alert_minutes);
                    ImGui::TextUnformatted("Upcoming starts:");
                    auto t = now;
                    for (int n = 0; n < 6; ++n) {
                        auto occurrence = events::next_start(e, t);
                        ImGui::TextUnformatted(time_label(occurrence).c_str()); t = occurrence + 1;
                    }
                    ImGui::EndTooltip();
                }
                ImGui::EndGroup();
                if (!e.waypoint.empty()) { ImGui::SameLine(); if (ImGui::SmallButton("Waypoint")) copy_waypoint(e.waypoint); }
                if (!e.wiki.empty()) { ImGui::SameLine(); if (ImGui::SmallButton("Wiki")) open_wiki(e.wiki); }
                ImGui::Separator();
                ImGui::PopID();
            }
            if (order.empty()) ImGui::TextUnformatted("No events match the current filters.");
        }
        ImGui::EndChild();
        if (ImGui::Button("Back to top")) scroll_to_top_requested = true;
    }
    ImGui::End();
}
void render() {
    try {
        if (toggle_requested.exchange(false)) visible = !visible;
        auto now = static_cast<std::int64_t>(std::time(nullptr));
        scan_alerts(now);
        draw_window(now);
        draw_notices(now);
    } catch (const std::exception& e) { log(LOGL_WARNING, e.what()); }
}
void render_options() {
    if (ImGui::Button("Open Events and Metas Observer")) visible = true;
    draw_controls();
}
void on_bind(const char*, bool released) {
    if (!released) toggle_requested.store(true);
}
void load(AddonAPI_t* provided) {
    api = provided;
    visible = false;
    scroll_to_top_requested = false;
    try {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(api->ImguiContext));
        ImGui::SetAllocatorFunctions(reinterpret_cast<void*(*)(size_t, void*)>(api->ImguiMalloc),
            reinterpret_cast<void(*)(void*, void*)>(api->ImguiFree));
        all_events = events::bundled_events();
        watched.assign(all_events.size(), 1);
        last_alerted.assign(all_events.size(), events::never_alerted);
        texture_ids.clear();
        for (const auto& event : all_events) {
            const char* identifier = texture_id;
            for (const auto& icon : bundled_icons::images)
                if (event.icon == icon.key) { identifier = icon.identifier; break; }
            texture_ids.emplace_back(identifier);
        }
        const char* folder = api->Paths_GetAddonDirectory("events-observer");
        if (!folder || !*folder) throw std::runtime_error("Nexus did not provide an addon directory");
        std::filesystem::create_directories(folder);
        settings_path = std::filesystem::path(folder) / "settings.txt";
        load_settings();
        last_check = events::never_alerted;
        if (api->Textures_GetOrCreateFromMemory)
            api->Textures_GetOrCreateFromMemory(texture_id, observer_icon, sizeof observer_icon);
        // Queue each distinct local PNG once, before the user opens the window.
        if (api->Textures_GetOrCreateFromMemory)
            for (auto& icon : bundled_icons::images)
                api->Textures_GetOrCreateFromMemory(icon.identifier, icon.data, icon.size);
        api->InputBinds_RegisterWithString(bind_id, on_bind, "CTRL+ALT+E");
        api->QuickAccess_Add(shortcut_id, texture_id, texture_id, bind_id, "Events and Metas Observer");
        api->GUI_RegisterCloseOnEscape(window_name, &visible);
        api->GUI_Register(RT_Render, render);
        api->GUI_Register(RT_OptionsRender, render_options);
        log(LOGL_INFO, "Loaded 88 events from Blish HUD Events and Metas Observer 1.0.9.");
    } catch (const std::exception& e) { log(LOGL_CRITICAL, e.what()); }
}
void unload() {
    if (!api) return;
    api->GUI_Deregister(render);
    api->GUI_Deregister(render_options);
    api->GUI_DeregisterCloseOnEscape(window_name);
    api->InputBinds_Deregister(bind_id);
    api->QuickAccess_Remove(shortcut_id);
    // No addon threads or texture receiver callbacks survive unloading.
    notices.clear();
    all_events.clear();
    watched.clear();
    last_alerted.clear();
    texture_ids.clear();
    toggle_requested.store(false);
    api = nullptr;
}
} // namespace

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    // Local signature, not registered in the public Nexus library.
    static AddonDefinition_t definition = {
        0x45564F42, NEXUS_API_VERSION, "Events and Metas Observer (Fedora port)",
        {0, 1, 4, 0}, "Community / local Nexus port",
        "Event reminders, watchlist and waypoints. Original Blish HUD schedule 1.0.9.",
        load, unload, AF_None, UP_None, nullptr
    };
    return &definition;
}
