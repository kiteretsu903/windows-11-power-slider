#include "power_controller.h"

#include <algorithm>

namespace {
constexpr GUID kEfficiencyMode{0x961cc777, 0x2547, 0x4f9d, {0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a}};
constexpr GUID kBalancedMode{};
constexpr GUID kPerformanceMode{0xded574b5, 0x45a0, 0x4f42, {0x87, 0x37, 0x46, 0x34, 0x5c, 0x09, 0xc2, 0x38}};


const GUID& guid_for_position(PowerModePosition position) noexcept {
    switch (position) {
    case PowerModePosition::Efficiency:
        return kEfficiencyMode;
    case PowerModePosition::Performance:
        return kPerformanceMode;
    case PowerModePosition::Balanced:
    default:
        return kBalancedMode;
    }
}

std::optional<PowerModePosition> position_for_guid(const GUID& value) noexcept {
    if (IsEqualGUID(value, kEfficiencyMode)) {
        return PowerModePosition::Efficiency;
    }
    if (IsEqualGUID(value, kPerformanceMode)) {
        return PowerModePosition::Performance;
    }
    if (IsEqualGUID(value, kBalancedMode)) {
        return PowerModePosition::Balanced;
    }
    return std::nullopt;
}

} // namespace

PowerController::PowerController() {
    module_ = LoadLibraryExW(L"powrprof.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module_) {
        last_error_ = GetLastError();
        return;
    }

    get_ac_ = reinterpret_cast<GetUserPowerModeFn>(GetProcAddress(module_, "PowerGetUserConfiguredACPowerMode"));
    get_dc_ = reinterpret_cast<GetUserPowerModeFn>(GetProcAddress(module_, "PowerGetUserConfiguredDCPowerMode"));
    set_ac_ = reinterpret_cast<SetUserPowerModeFn>(GetProcAddress(module_, "PowerSetUserConfiguredACPowerMode"));
    set_dc_ = reinterpret_cast<SetUserPowerModeFn>(GetProcAddress(module_, "PowerSetUserConfiguredDCPowerMode"));
    get_active_scheme_ = reinterpret_cast<GetActiveSchemeFn>(GetProcAddress(module_, "PowerGetActiveScheme"));
    read_dc_value_index_ = reinterpret_cast<ReadDcValueIndexFn>(GetProcAddress(module_, "PowerReadDCValueIndex"));

    if (!available()) {
        last_error_ = ERROR_PROC_NOT_FOUND;
    }
}

PowerController::~PowerController() {
    if (module_) {
        FreeLibrary(module_);
    }
}

bool PowerController::available() const noexcept {
    return module_ && get_ac_ && get_dc_ && set_ac_ && set_dc_ && get_active_scheme_ &&
           read_dc_value_index_;
}

SupplyKind PowerController::supply_kind() const noexcept {
    SYSTEM_POWER_STATUS status{};
    if (!GetSystemPowerStatus(&status)) {
        return SupplyKind::Unknown;
    }
    if (status.ACLineStatus == 0) {
        return SupplyKind::Battery;
    }
    if (status.ACLineStatus == 1) {
        return SupplyKind::Ac;
    }
    return SupplyKind::Unknown;
}

bool PowerController::energy_saver_active() const noexcept {
    SYSTEM_POWER_STATUS status{};
    return GetSystemPowerStatus(&status) && status.ACLineStatus == 0 && status.SystemStatusFlag != 0;
}

std::optional<DWORD> PowerController::critical_battery_threshold() const noexcept {
    if(!get_active_scheme_ || !read_dc_value_index_) return std::nullopt;
    constexpr GUID subgroup{0xe73a048d,0xbf27,0x4f12,{0x97,0x31,0x8b,0x20,0x76,0xe8,0x89,0x1f}};
    constexpr GUID setting{0x9a66d8d7,0x4ff7,0x4ef9,{0xb5,0xa2,0x5a,0x32,0x6c,0xa2,0xa4,0x69}};
    GUID* scheme=nullptr;
    DWORD error=get_active_scheme_(nullptr,&scheme);
    if(error!=ERROR_SUCCESS || !scheme) {if(scheme)LocalFree(scheme);return std::nullopt;}
    DWORD value=0;
    error=read_dc_value_index_(nullptr,scheme,&subgroup,&setting,&value);
    LocalFree(scheme);
    if(error!=ERROR_SUCCESS || value>100) return std::nullopt;
    return value;
}

std::optional<PowerModePosition> PowerController::get_mode(GetUserPowerModeFn fn) const noexcept {
    if (!fn) {
        last_error_ = ERROR_PROC_NOT_FOUND;
        return std::nullopt;
    }
    GUID value{};
    const DWORD result = fn(&value);
    if (result != ERROR_SUCCESS) {
        last_error_ = result;
        return std::nullopt;
    }
    return position_for_guid(value);
}

std::optional<PowerModePosition> PowerController::ac_mode() const noexcept {
    return get_mode(get_ac_);
}

std::optional<PowerModePosition> PowerController::dc_mode() const noexcept {
    return get_mode(get_dc_);
}

int PowerController::battery_ui_position() const noexcept {
    const auto mode=dc_mode();
    return mode?static_cast<int>(*mode):1;
}

bool PowerController::set_mode(SetUserPowerModeFn fn, PowerModePosition position) noexcept {
    if (!fn) {
        last_error_ = ERROR_PROC_NOT_FOUND;
        return false;
    }
    const GUID& value = guid_for_position(position);
    last_error_ = fn(&value);
    return last_error_ == ERROR_SUCCESS;
}

bool PowerController::set_ac_mode(PowerModePosition position) noexcept {
    return set_mode(set_ac_, position);
}

bool PowerController::set_battery_ui_position(int position) noexcept {
    if(position<0 || position>2) {last_error_=ERROR_INVALID_PARAMETER;return false;}
    if(energy_saver_active()) {last_error_=ERROR_BUSY;return false;}
    return set_mode(set_dc_,static_cast<PowerModePosition>(position));
}

DWORD PowerController::last_error() const noexcept {
    return last_error_;
}
