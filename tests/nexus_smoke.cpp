#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Nexus.h"
#include "imgui.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

std::vector<std::pair<ERenderType, GUI_RENDER>> callbacks;
INPUTBINDS_PROCESS bind = nullptr;
bool shortcut = false;
bool* escape_visible = nullptr;
void* allocate(size_t size, void*) { return std::malloc(size); }
void release(void* p, void*) { std::free(p); }
void add_render(ERenderType type, GUI_RENDER render) { callbacks.emplace_back(type, render); }
void remove_render(GUI_RENDER render) {
    callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(), [render](const auto& p) { return p.second == render; }), callbacks.end());
}
void register_escape(const char*, bool* p) { escape_visible = p; }
void deregister_escape(const char*) { escape_visible = nullptr; }
const char* addon_path(const char*) { return "smoke-settings"; }
void logger(ELogLevel level, const char*, const char* text) {
    std::printf("addon log %d: %s\n", level, text);
    assert(level != LOGL_CRITICAL);
}
void alert(const char* text) { std::printf("alert: %s\n", text); }
void register_bind(const char*, INPUTBINDS_PROCESS handler, const char*) { bind = handler; }
void deregister_bind(const char*) { bind = nullptr; }
void add_shortcut(const char*, const char*, const char*, const char*, const char*) { shortcut = true; }
void remove_shortcut(const char*) { shortcut = false; }
Texture_t* texture_memory(const char*, void*, uint64_t) { return nullptr; }
Texture_t* texture_url(const char*, const char*, const char*) { return nullptr; }
void frame() {
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1440, 900);
    io.DeltaTime = 1.f / 60.f;
    ImGui::NewFrame();
    for (auto callback : callbacks) {
        if (callback.first == RT_OptionsRender) {
            ImGui::Begin("Mock Nexus Options"); callback.second(); ImGui::End();
        } else callback.second();
    }
    ImGui::Render();
    assert(ImGui::GetDrawData());
}
int main() {
    std::filesystem::create_directories("smoke-settings");
    std::ofstream("smoke-settings/settings.txt") << "version 1\nnotifications 0\nchime 0\nposition 180 60\n";
    ImGui::SetAllocatorFunctions(allocate, release);
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    unsigned char* pixels; int width, height;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    AddonAPI_t api{};
    api.ImguiContext = ImGui::GetCurrentContext();
    api.ImguiMalloc = reinterpret_cast<void*>(allocate);
    api.ImguiFree = reinterpret_cast<void*>(release);
    api.GUI_Register = add_render; api.GUI_Deregister = remove_render;
    api.GUI_RegisterCloseOnEscape = register_escape;
    api.GUI_DeregisterCloseOnEscape = deregister_escape;
    api.Paths_GetAddonDirectory = addon_path; api.Log = logger; api.GUI_SendAlert = alert;
    api.InputBinds_RegisterWithString = register_bind; api.InputBinds_Deregister = deregister_bind;
    api.QuickAccess_Add = add_shortcut; api.QuickAccess_Remove = remove_shortcut;
    api.Textures_GetOrCreateFromMemory = texture_memory; api.Textures_GetOrCreateFromURL = texture_url;
    for (int cycle = 0; cycle < 3; ++cycle) {
        auto library = LoadLibraryW(L"events-observer.dll");
        if (!library) { std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError()); return 1; }
        auto get_definition = reinterpret_cast<AddonDefinition_t*(*)()>(GetProcAddress(library, "GetAddonDef"));
        assert(get_definition);
        auto* definition = get_definition();
        assert(definition && definition->APIVersion == 6 && definition->Signature == 0x45564F42);
        definition->Load(&api);
        assert(callbacks.size() == 2 && bind && shortcut && escape_visible);
        frame(); frame();
        bool original_visible = *escape_visible;
        bind("test", true); frame(); assert(*escape_visible == original_visible);
        bind("test", false); frame(); assert(*escape_visible != original_visible);
        bind("test", false); frame(); assert(*escape_visible == original_visible);
        definition->Unload();
        assert(callbacks.empty() && !bind && !shortcut && !escape_visible);
        assert(FreeLibrary(library));
    }
    ImGui::DestroyContext();
    std::puts("Nexus smoke passed: real DLL load/render/options/keybind/unload, 3 cycles.");
}
