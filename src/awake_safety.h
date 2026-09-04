#pragma once
#include <windows.h>
#include <algorithm>

enum class AwakeStopReason { None, LowBattery, UnknownPower };

inline DWORD awake_release_threshold(DWORD critical) noexcept {
    return std::max<DWORD>(5,std::min<DWORD>(critical,98)+2);
}

inline AwakeStopReason awake_stop_reason(bool readable,const SYSTEM_POWER_STATUS& status,DWORD critical) noexcept {
    if(!readable) return AwakeStopReason::UnknownPower;
    // Confirmed mains power and desktops without a battery are safe to keep awake.
    if(status.ACLineStatus==1) return AwakeStopReason::None;
    if(status.BatteryFlag!=255 && (status.BatteryFlag&128)) return AwakeStopReason::None;
    if(status.BatteryFlag!=255 && (status.BatteryFlag&4)) return AwakeStopReason::LowBattery;
    if(status.BatteryLifePercent<=100 && status.BatteryLifePercent<=awake_release_threshold(critical))
        return AwakeStopReason::LowBattery;
    // Without confirmed AC, missing battery telemetry must fail safe.
    if(status.BatteryLifePercent>100 || status.ACLineStatus!=0) return AwakeStopReason::UnknownPower;
    return AwakeStopReason::None;
}
