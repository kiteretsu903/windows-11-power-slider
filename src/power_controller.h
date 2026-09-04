#pragma once

#include <windows.h>
#include <optional>

enum class SupplyKind {
    Battery,
    Ac,
    Unknown,
};

enum class PowerModePosition : int {
    Efficiency = 0,
    Balanced = 1,
    Performance = 2,
};

class PowerController {
public:
    PowerController();
    ~PowerController();

    PowerController(const PowerController&) = delete;
    PowerController& operator=(const PowerController&) = delete;

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] SupplyKind supply_kind() const noexcept;
    [[nodiscard]] bool energy_saver_active() const noexcept;
    [[nodiscard]] std::optional<DWORD> critical_battery_threshold() const noexcept;

    [[nodiscard]] std::optional<PowerModePosition> ac_mode() const noexcept;
    [[nodiscard]] std::optional<PowerModePosition> dc_mode() const noexcept;
    [[nodiscard]] int battery_ui_position() const noexcept;

    [[nodiscard]] bool set_ac_mode(PowerModePosition position) noexcept;
    [[nodiscard]] bool set_battery_ui_position(int position) noexcept;
    [[nodiscard]] DWORD last_error() const noexcept;

private:
    using GetUserPowerModeFn = DWORD(WINAPI*)(GUID*);
    using SetUserPowerModeFn = DWORD(WINAPI*)(const GUID*);
    using GetActiveSchemeFn = DWORD(WINAPI*)(HKEY, GUID**);
    using ReadDcValueIndexFn = DWORD(WINAPI*)(HKEY, const GUID*, const GUID*, const GUID*, DWORD*);

    [[nodiscard]] std::optional<PowerModePosition> get_mode(GetUserPowerModeFn fn) const noexcept;
    [[nodiscard]] bool set_mode(SetUserPowerModeFn fn, PowerModePosition position) noexcept;

    HMODULE module_{};
    GetUserPowerModeFn get_ac_{};
    GetUserPowerModeFn get_dc_{};
    SetUserPowerModeFn set_ac_{};
    SetUserPowerModeFn set_dc_{};
    GetActiveSchemeFn get_active_scheme_{};
    ReadDcValueIndexFn read_dc_value_index_{};
    mutable DWORD last_error_{};
};
