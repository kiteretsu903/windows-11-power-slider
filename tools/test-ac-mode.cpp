#include "power_controller.h"
#include <cstdio>

// Integration test: exercise the production controller and restore the exact
// original AC GUID, including modes unknown to the application's enum.
int main() {
    HMODULE module = LoadLibraryExW(L"powrprof.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return 1;
    using Get = DWORD(WINAPI*)(GUID*);
    using Set = DWORD(WINAPI*)(const GUID*);
    auto get = reinterpret_cast<Get>(GetProcAddress(module, "PowerGetUserConfiguredACPowerMode"));
    auto set = reinterpret_cast<Set>(GetProcAddress(module, "PowerSetUserConfiguredACPowerMode"));
    if (!get || !set) return 2;
    GUID original{};
    if (get(&original) != ERROR_SUCCESS) return 3;
    PowerController controller;
    const bool changed = controller.set_ac_mode(PowerModePosition::Efficiency);
    const DWORD error = controller.last_error();
    GUID actual{};
    const GUID expected{0x961cc777, 0x2547, 0x4f9d, {0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a}};
    const bool verified = get(&actual) == ERROR_SUCCESS && IsEqualGUID(actual, expected);
    const bool decoded = controller.ac_mode() == PowerModePosition::Efficiency;
    const DWORD restore = set(&original);
    GUID restored{};
    const bool restored_ok = restore == ERROR_SUCCESS && get(&restored) == ERROR_SUCCESS && IsEqualGUID(restored, original);
    std::printf("Set=%d Error=%lu ReadBack=%d Decode=%d Restored=%d\n", changed, error, verified, decoded, restored_ok);
    FreeLibrary(module);
    return changed && verified && decoded && restored_ok ? 0 : 4;
}
