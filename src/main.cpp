#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <winternl.h>
#include <strsafe.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "power_controller.h"
#include "renderer.h"
#include "diagnostics.h"
#include "tray_icon.h"
#include "tray_click.h"
#include "awake_safety.h"
#include "native_motion.h"

namespace {
constexpr wchar_t kWindowClass[] = L"PowerModeNative.Flyout";
constexpr wchar_t kAppName[] = L"Windows 11 Power Slider";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"PowerModeNative";

constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kShowExistingMessage = WM_APP + 2;
constexpr UINT kRestoreBackdropMessage = WM_APP + 3;
constexpr UINT kMotionFrameMessage = WM_APP + 4;
constexpr UINT kExitExistingMessage = WM_APP + 6;
constexpr UINT_PTR kRefreshTimer = 1;
constexpr UINT kCmdStartup = 1001;
constexpr UINT kCmdExit = 1002;

constexpr int kLogicalWidth = 392;
constexpr int kLogicalHeight = 370;
constexpr wchar_t kPreferences[] = L"Software\\PowerModeNative";
constexpr UINT kCmdLanguageAuto = 1003;
constexpr UINT kCmdLanguageZh = 1004;
constexpr UINT kCmdLanguageEn = 1005;

constexpr int kDwmUseImmersiveDarkMode = 20;
constexpr int kDwmWindowCornerPreference = 33;
constexpr int kDwmBorderColor = 34;
constexpr int kDwmSystemBackdropType = 38;
constexpr int kDwmRound = 2;
constexpr COLORREF kDwmColorNone = 0xFFFFFFFE;

struct SliderLayout {
    RECT hit{};
    int rail_left{};
    int rail_right{};
    int rail_y{};
    int positions{};
};

std::wstring executable_path() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

bool startup_enabled() {
    DWORD type = 0;
    DWORD bytes = 0;
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, &type, nullptr, &bytes) == ERROR_SUCCESS;
}

bool set_startup_enabled(bool enabled) {
    if (!enabled) {
        const LSTATUS result = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue);
        return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }

    const std::wstring path = executable_path();
    if (path.empty()) {
        return false;
    }
    const std::wstring command = L"\"" + path + L"\" --startup";
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, REG_SZ,
                           command.c_str(), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool use_light_theme() {
    DWORD value = 1;
    DWORD bytes = sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &bytes);
    return value != 0;
}

DWORD os_build_number() {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto rtl_get_version = ntdll
        ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;
    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    return rtl_get_version && rtl_get_version(&version) == 0 ? version.dwBuildNumber : 0;
}

HICON make_tray_icon(int mode, bool awake) {
    return make_mode_tray_icon(mode,awake,taskbar_icon_size());
}

class App {
public:
    int run(HINSTANCE instance, int show_command) {
        (void)show_command;
        preview_=wcsstr(GetCommandLineW(),L"--preview")!=nullptr;
        const bool shutdown_requested = wcsstr(GetCommandLineW(), L"--shutdown") != nullptr;
        instance_ = instance;
        taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool uninitialize_com = SUCCEEDED(com_result);

        mutex_ = CreateMutexW(nullptr, FALSE, L"Local\\PowerModeNative.Singleton");
        if (mutex_ && GetLastError() == ERROR_ALREADY_EXISTS) {
            if (HWND existing = FindWindowW(kWindowClass, nullptr)) {
                PostMessageW(existing, shutdown_requested ? kExitExistingMessage : kShowExistingMessage, 0, 0);
            }
            if (uninitialize_com) CoUninitialize();
            return 0;
        }
        if (shutdown_requested) {
            if (mutex_) CloseHandle(mutex_);
            mutex_ = nullptr;
            if (uninitialize_com) CoUninitialize();
            return 0;
        }

        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_DBLCLKS;
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance_;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(101));
        window_class.hIconSm = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(101),
            IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        window_class.hbrBackground = nullptr;
        window_class.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&window_class)) {
            if (uninitialize_com) CoUninitialize();
            return 1;
        }

        hwnd_ = CreateWindowExW((preview_?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW) | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP,
                                kWindowClass, L"Power Mode", WS_POPUP,
                                CW_USEDEFAULT, CW_USEDEFAULT, kLogicalWidth, kLogicalHeight,
                                nullptr, nullptr, instance_, this);
        if (!hwnd_) {
            if (uninitialize_com) CoUninitialize();
            return 2;
        }

        create_tray_icon();
        // Repair the stored path after moving from portable to installed build,
        // but preserve a user's explicit choice to disable startup.
        if(startup_enabled()) set_startup_enabled(true);
        DWORD initialized=0, bytes=sizeof(initialized);
        if(RegGetValueW(HKEY_CURRENT_USER,kPreferences,L"StartupInitialized",RRF_RT_REG_DWORD,
                        nullptr,&initialized,&bytes)!=ERROR_SUCCESS || !initialized) {
            if(set_startup_enabled(true)) {
                initialized=1;
                RegSetKeyValueW(HKEY_CURRENT_USER,kPreferences,L"StartupInitialized",REG_DWORD,&initialized,sizeof(initialized));
            }
        }
        SetTimer(hwnd_, kRefreshTimer, 1500, nullptr);
        refresh_state(true);
        if (preview_ || wcsstr(GetCommandLineW(), L"--show")) {
            show_flyout();
        }

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (uninitialize_com) CoUninitialize();
        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
        App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
            self = static_cast<App*>(create->lpCreateParams);
            self->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->handle_message(message, w_param, l_param)
                    : DefWindowProcW(hwnd, message, w_param, l_param);
    }

    LRESULT handle_message(UINT message, WPARAM w_param, LPARAM l_param) {
        if (taskbar_created_message_ && message == taskbar_created_message_) {
            tray_added_ = false;
            create_tray_icon();
            if (tray_added_) update_tray_icon();
            return 0;
        }
        switch (message) {
        case WM_CREATE:
            dpi_ = GetDpiForWindow(hwnd_);
            { DWORD bytes=sizeof(language_);
              RegGetValueW(HKEY_CURRENT_USER,kPreferences,L"Language",RRF_RT_REG_DWORD,nullptr,&language_,&bytes);
              if(language_>2) language_=0; }
            light_theme_ = use_light_theme();
            apply_backdrop();
            renderer_.initialize(hwnd_, dpi_);
            return 0;

        case WM_DPICHANGED: {
            dpi_ = HIWORD(w_param);
            const RECT* suggested = reinterpret_cast<RECT*>(l_param);
            SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
            RECT client{};
            GetClientRect(hwnd_, &client);
            renderer_.resize(client.right - client.left, client.bottom - client.top, dpi_);
            InvalidateRect(hwnd_, nullptr, TRUE);
            return 0;
        }

        case WM_SIZE:
            renderer_.resize(LOWORD(l_param), HIWORD(l_param), dpi_);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            light_theme_ = use_light_theme();
            update_tray_icon();
            apply_backdrop();
            InvalidateRect(hwnd_, nullptr, TRUE);
            return 0;

        case WM_POWERBROADCAST:
            if(w_param==PBT_APMSUSPEND || w_param==PBT_APMRESUMEAUTOMATIC ||
               w_param==PBT_APMRESUMESUSPEND || w_param==PBT_APMRESUMECRITICAL) {
                // Never reinstate wakefulness after a system sleep transition.
                awake_release_pending_=!clear_keep_awake();
                update_tray_icon();
            }
            refresh_state(true);
            return TRUE;

        case WM_TIMER:
            if (w_param == kRefreshTimer) {
                if (!tray_added_) {
                    create_tray_icon();
                    if (tray_added_) update_tray_icon();
                }
                if(dragging_) guard_keep_awake();
                else refresh_state(false);
            }
            return 0;

        case WM_PAINT:
            paint();
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_LBUTTONDOWN:
            if(closing_) return 0;
            on_left_button_down(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            return 0;

        case WM_MOUSEMOVE:
            { int x=GET_X_LPARAM(l_param),y=GET_Y_LPARAM(l_param);
              int next=point_in_rect(keep_awake_rect(),x,y)?2:-1;
              if(next!=hover_) { hover_=next; InvalidateRect(hwnd_,nullptr,FALSE); }
              TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd_,0}; TrackMouseEvent(&track); }
            if (dragging_) on_drag(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            return 0;
        case WM_MOUSELEAVE:
            hover_=-1; InvalidateRect(hwnd_,nullptr,FALSE); return 0;

        case WM_LBUTTONUP:
            on_left_button_up(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
            return 0;

        case WM_KEYDOWN:
            if (w_param == VK_ESCAPE) hide_flyout();
            if (w_param == VK_APPS) show_tray_menu();
            return 0;
        case WM_CONTEXTMENU:
            show_tray_menu(); return 0;

        case WM_ACTIVATE:
            trace_event("activate",w_param,visible_);
            if (LOWORD(w_param) == WA_INACTIVE && visible_ && !menu_open_ && !preview_) {
                if((GetAsyncKeyState(VK_LBUTTON)&0x8000) && pointer_on_tray_icon())
                    tray_click_.focus_lost_on_icon(GetTickCount64());
                hide_flyout();
            }
            return DefWindowProcW(hwnd_, message, w_param, l_param);

        case kTrayMessage:
            if (const UINT tray_event = LOWORD(static_cast<DWORD_PTR>(l_param));
                tray_event == WM_LBUTTONDOWN || tray_event == WM_LBUTTONDBLCLK) {
                tray_click_.down(visible_,GetTickCount64());
            } else if (tray_event == WM_LBUTTONUP) {
                tray_click_.up(visible_,GetTickCount64()) ? hide_flyout() : show_flyout();
            } else if (tray_event == WM_RBUTTONUP || tray_event == WM_CONTEXTMENU) {
                show_tray_menu();
            }
            return 0;

        case kShowExistingMessage:
            show_flyout();
            return 0;

        case kExitExistingMessage:
            DestroyWindow(hwnd_);
            return 0;

        case kRestoreBackdropMessage:
            trace_event("restore",visible_,acrylic_);
            if(visible_) apply_backdrop();
            return 0;

        case kMotionFrameMessage:
            if(motion_active_ && w_param==motion_generation_) advance_motion();
            return 0;

        case WM_COMMAND:
            if(LOWORD(w_param)>=kCmdLanguageAuto && LOWORD(w_param)<=kCmdLanguageEn) {
                set_language(LOWORD(w_param)-kCmdLanguageAuto); return 0;
            }
            if (LOWORD(w_param) == kCmdStartup) {
                if (!set_startup_enabled(!startup_enabled())) {
                    show_balloon(chinese()?L"无法更新开机启动设置。":L"Could not update startup settings.", NIIF_WARNING);
                }
            } else if (LOWORD(w_param) == kCmdExit) {
                DestroyWindow(hwnd_);
            }
            return 0;

        case WM_QUERYENDSESSION:
            clear_keep_awake();
            return TRUE;

        case WM_ENDSESSION:
            if (w_param) {
                clear_keep_awake();
                DestroyWindow(hwnd_);
            }
            return 0;

        case WM_CLOSE:
            hide_flyout();
            return 0;

        case WM_DESTROY:
            cleanup();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd_, message, w_param, l_param);
    }

    int scale(int value) const noexcept {
        return MulDiv(value, static_cast<int>(dpi_), 96);
    }

    RECT logical_rect(int left, int top, int right, int bottom) const noexcept {
        return RECT{scale(left), scale(top), scale(right), scale(bottom)};
    }

    SliderLayout slider_layout(bool battery) const noexcept {
        const int y = battery ? 244 : 120;
        SliderLayout layout{};
        layout.hit = logical_rect(28, y - 18, 364, y + 34);
        layout.rail_left = scale(40);
        layout.rail_right = scale(352);
        layout.rail_y = scale(y);
        layout.positions = 3;
        return layout;
    }

    RECT keep_awake_rect() const noexcept {
        return logical_rect(16, 306, 376, 354);
    }

    static bool point_in_rect(const RECT& rect, int x, int y) noexcept {
        POINT point{x, y};
        return PtInRect(&rect, point) != FALSE;
    }

    int nearest_position(const SliderLayout& layout, int x) const noexcept {
        const int clamped = std::clamp(x, layout.rail_left, layout.rail_right);
        const double fraction = static_cast<double>(clamped - layout.rail_left) /
                                static_cast<double>(layout.rail_right - layout.rail_left);
        return std::clamp(static_cast<int>(std::lround(fraction * (layout.positions - 1))),
                          0, layout.positions - 1);
    }

    void on_left_button_down(int x, int y) {
        const SliderLayout ac = slider_layout(false);
        const SliderLayout battery = slider_layout(true);
        if (point_in_rect(ac.hit, x, y)) {
            dragging_ = true;
            dragging_battery_ = false;
            drag_position_ = nearest_position(ac, x);
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (point_in_rect(battery.hit, x, y)) {
            dragging_ = true;
            dragging_battery_ = true;
            drag_position_ = nearest_position(battery, x);
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (point_in_rect(keep_awake_rect(), x, y)) {
            toggle_keep_awake();
        }
    }

    void on_drag(int x, int) {
        const SliderLayout layout = slider_layout(dragging_battery_);
        const int next = nearest_position(layout, x);
        if (next != drag_position_) {
            drag_position_ = next;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void on_left_button_up(int, int) {
        if (!dragging_) return;
        ReleaseCapture();
        dragging_ = false;

        bool success = false;
        if (dragging_battery_) {
            success = power_.set_battery_ui_position(drag_position_);
        } else {
            success = power_.set_ac_mode(static_cast<PowerModePosition>(drag_position_));
        }

        if (!success) {
            if (power_.last_error() == ERROR_BUSY) {
                show_balloon(chinese()?L"系统自动启用了省电模式，请先在快速设置中关闭。":L"Turn off Energy Saver in Quick Settings first.", NIIF_INFO);
            } else {
                show_balloon(chinese()?L"Windows 拒绝了这次电源模式更改。":L"Windows could not change the power mode.", NIIF_WARNING);
            }
        }
        refresh_state(true);
    }

    void toggle_keep_awake() {
        const bool target = !keep_awake_;
        if(target) {
            const auto reason=keep_awake_stop_reason();
            if(reason!=AwakeStopReason::None || awake_release_pending_) {
                show_balloon(reason==AwakeStopReason::LowBattery?
                    (chinese()?L"电池电量过低，暂不允许保持唤醒。":L"Battery is too low to enable Keep awake."):
                    (chinese()?L"暂时无法确认安全供电状态，请稍后重试。":L"Cannot confirm safe power status. Please try again later."),NIIF_WARNING);
                return;
            }
        }
        const EXECUTION_STATE state = target
            ? SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)
            : SetThreadExecutionState(ES_CONTINUOUS);
        if (state == 0) {
            show_balloon(chinese()?L"无法更改保持唤醒状态。":L"Could not change Keep awake.", NIIF_WARNING);
            return;
        }
        keep_awake_ = target;
        awake_release_pending_=false;
        awake_warning_shown_=false;
        update_tray_icon();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    bool clear_keep_awake() {
        if (keep_awake_) {
            if(!SetThreadExecutionState(ES_CONTINUOUS)) return false;
            keep_awake_ = false;
        }
        return true;
    }

    AwakeStopReason keep_awake_stop_reason() const {
        SYSTEM_POWER_STATUS status{};
        const bool readable=GetSystemPowerStatus(&status)!=FALSE;
        return awake_stop_reason(readable,status,power_.critical_battery_threshold().value_or(3));
    }

    void guard_keep_awake() {
        if(!keep_awake_) return;
        const auto reason=keep_awake_stop_reason();
        if(reason==AwakeStopReason::None && !awake_release_pending_) return;
        if(!clear_keep_awake()) {
            awake_release_pending_=true;
            if(!awake_warning_shown_) show_balloon(chinese()?L"无法解除保持唤醒，正在重试。请保存工作并接通电源。":
                L"Could not release Keep awake; retrying. Save your work and connect power.",NIIF_WARNING);
            awake_warning_shown_=true;
            return;
        }
        awake_release_pending_=false;
        awake_warning_shown_=false;
        update_tray_icon();
        InvalidateRect(hwnd_,nullptr,FALSE);
        show_balloon(reason==AwakeStopReason::LowBattery?
            (chinese()?L"电池电量过低，已关闭保持唤醒，让 Windows 执行电源保护。":L"Low battery: Keep awake is off so Windows can apply its power protection."):
            (chinese()?L"供电状态不确定，已安全关闭保持唤醒。":L"Power status is uncertain. Keep awake has been turned off for safety."),NIIF_INFO);
    }

    void refresh_state(bool force_repaint) {
        guard_keep_awake();
        // Also resync on open and during the existing refresh timer, in case
        // Windows' theme-change broadcast was missed while the flyout hid.
        const bool light=use_light_theme();
        if(light!=light_theme_) {
            light_theme_=light;
            apply_backdrop();
            force_repaint=true;
        }
        const int previous_ac = ac_position_;
        const int previous_battery = battery_position_;
        const SupplyKind previous_supply = supply_;
        const bool previous_saver = energy_saver_active_;

        if (const auto mode = power_.ac_mode()) ac_position_ = static_cast<int>(*mode);
        battery_position_ = power_.battery_ui_position();
        supply_ = power_.supply_kind();
        energy_saver_active_ = power_.energy_saver_active();

        if (force_repaint || previous_ac != ac_position_ || previous_battery != battery_position_ ||
            previous_supply != supply_ || previous_saver != energy_saver_active_) {
            update_tray_icon();
            InvalidateRect(hwnd_, nullptr, TRUE);
        }
    }

    void apply_backdrop() {
        const BOOL disable_legacy_animation=TRUE;
        DwmSetWindowAttribute(hwnd_,DWMWA_TRANSITIONS_FORCEDISABLED,
            &disable_legacy_animation,sizeof(disable_legacy_animation));
        const BOOL dark = light_theme_ ? FALSE : TRUE;
        DwmSetWindowAttribute(hwnd_, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
        const int corner = kDwmRound;
        DwmSetWindowAttribute(hwnd_, kDwmWindowCornerPreference, &corner, sizeof(corner));
        const COLORREF border = kDwmColorNone;
        DwmSetWindowAttribute(hwnd_, kDwmBorderColor, &border, sizeof(border));

        // Do not stack system material over the controllable accent backdrop.
        const int none=1;
        DwmSetWindowAttribute(hwnd_,kDwmSystemBackdropType,&none,sizeof(none));
        MARGINS margins{};
        DwmExtendFrameIntoClientArea(hwnd_,&margins);
        struct AccentPolicy { int state; int flags; DWORD color; int animation; };
        struct CompositionData { int attribute; void* data; SIZE_T size; };
        using SetComposition=BOOL(WINAPI*)(HWND,CompositionData*);
        const auto set=reinterpret_cast<SetComposition>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetWindowCompositionAttribute"));
        DWORD transparency=1,bytes=sizeof(transparency);
        RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"EnableTransparency",RRF_RT_REG_DWORD,nullptr,&transparency,&bytes);
        HIGHCONTRASTW contrast{};
        contrast.cbSize=sizeof(contrast);
        SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);
        const bool enabled=os_build_number()>=22000 && transparency && !(contrast.dwFlags&HCF_HIGHCONTRASTON);
        // Alpha must be nonzero for the acrylic accent state.
        AccentPolicy policy{enabled?4:0,0,light_theme_?0x01F2F2F2u:0x011F1F1Fu,0};
        CompositionData data{19,&policy,sizeof(policy)};
        acrylic_=set && set(hwnd_,&data) && enabled;
        trace_event("accent",acrylic_,enabled);
    }

    void paint() {
        PAINTSTRUCT paint_struct{};
        BeginPaint(hwnd_, &paint_struct);
        EndPaint(hwnd_, &paint_struct);
        if(visible_) draw_frame();
    }

    bool draw_frame() {
        RenderState state{};
        state.ac_position = dragging_ && !dragging_battery_ ? drag_position_ : ac_position_;
        state.battery_position = dragging_ && dragging_battery_ ? drag_position_ : battery_position_;
        state.ac_active = supply_ == SupplyKind::Ac;
        state.battery_active = supply_ == SupplyKind::Battery;
        state.energy_saver_active = energy_saver_active_;
        state.keep_awake = keep_awake_;
        state.light_theme = light_theme_;
        state.acrylic = acrylic_;
        state.chinese = chinese();
        state.hover = hover_;
        const bool drawn=renderer_.draw(state);
        if(drawn && restore_backdrop_pending_) {
            restore_backdrop_pending_=false;
            PostMessageW(hwnd_,kRestoreBackdropMessage,0,0);
        }
        return drawn;
    }

    void create_tray_icon() {
        if (tray_icon_) DestroyIcon(tray_icon_);
        tray_icon_ = nullptr;
        tray_ = {};
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = hwnd_;
        tray_.uID = 1;
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        tray_.uCallbackMessage = kTrayMessage;
        tray_icon_ = make_tray_icon(2, false);
        tray_.hIcon = tray_icon_;
        StringCchCopyW(tray_.szTip, _countof(tray_.szTip), L"Windows 11 Power Slider");
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
        // Keep the legacy mouse callback protocol used by TrayClick.
        // Mixing button-up and version-4 selection events toggles twice.
    }

    void update_tray_icon() {
        int display_mode = energy_saver_active_ ? 0 :
            (supply_ == SupplyKind::Battery ? battery_position_ + 1 : ac_position_ + 1);
        HICON next = make_tray_icon(display_mode, keep_awake_);
        if (!next) return;
        if (!tray_added_) {
            DestroyIcon(next);
            return;
        }
        HICON previous = tray_icon_;
        tray_icon_ = next;
        tray_.uFlags = NIF_ICON | NIF_TIP;
        tray_.hIcon = tray_icon_;
        const wchar_t* source = supply_ == SupplyKind::Battery ? (chinese()?L"电池":L"Battery") : (chinese()?L"插电":L"Plugged in");
        const wchar_t* zhModes[]{L"省电模式",L"最佳能效",L"平衡",L"最佳性能"};
        const wchar_t* enModes[]{L"Saver",L"Efficiency",L"Balanced",L"Performance"};
        StringCchPrintfW(tray_.szTip, _countof(tray_.szTip), L"Windows 11 Power Slider · %s · %s%s",
                         source,(chinese()?zhModes:enModes)[std::clamp(display_mode,0,3)],
                         keep_awake_ ? (chinese()?L" · 保持唤醒":L" · Keep awake") : L"");
        if (!Shell_NotifyIconW(NIM_MODIFY, &tray_)) tray_added_ = false;
        if (previous) DestroyIcon(previous);
    }

    void show_balloon(const wchar_t* text, DWORD icon) {
        if (!tray_added_) return;
        tray_.uFlags = NIF_INFO;
        StringCchCopyW(tray_.szInfoTitle, _countof(tray_.szInfoTitle), kAppName);
        StringCchCopyW(tray_.szInfo, _countof(tray_.szInfo), text);
        tray_.dwInfoFlags = icon;
        Shell_NotifyIconW(NIM_MODIFY, &tray_);
    }

    void show_tray_menu() {
        menu_open_ = true;
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING | (startup_enabled() ? MF_CHECKED : MF_UNCHECKED),
                    kCmdStartup, chinese()?L"开机启动":L"Start with Windows");
        HMENU languages=CreatePopupMenu();
        AppendMenuW(languages,MF_STRING|(language_==0?MF_CHECKED:0),kCmdLanguageAuto,chinese()?L"跟随系统":L"System default");
        AppendMenuW(languages,MF_STRING|(language_==1?MF_CHECKED:0),kCmdLanguageZh,L"简体中文");
        AppendMenuW(languages,MF_STRING|(language_==2?MF_CHECKED:0),kCmdLanguageEn,L"English");
        AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(languages),chinese()?L"语言":L"Language");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kCmdExit, chinese()?L"退出":L"Exit");
        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(hwnd_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                       point.x, point.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);
        PostMessageW(hwnd_, WM_NULL, 0, 0);
        menu_open_ = false;
    }

    bool pointer_on_tray_icon() const noexcept {
        NOTIFYICONIDENTIFIER icon{};
        icon.cbSize=sizeof(icon);icon.hWnd=hwnd_;icon.uID=tray_.uID;
        RECT rect{};POINT cursor{};
        return SUCCEEDED(Shell_NotifyIconGetRect(&icon,&rect)) &&
            GetCursorPos(&cursor) && PtInRect(&rect,cursor);
    }

    void show_flyout() {
        trace_event("show-start",visible_,acrylic_);
        if(visible_ && !closing_) { SetForegroundWindow(hwnd_); return; }
        cancel_motion();
        if(closing_) {
            RECT from{};GetWindowRect(hwnd_,&from);
            closing_=false;
            SetForegroundWindow(hwnd_);
            SetFocus(hwnd_);
            if(animations_enabled()) start_motion(from.top,resting_y_,false);
            else SetWindowPos(hwnd_,nullptr,resting_x_,resting_y_,0,0,
                SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            return;
        }
        closing_=false;
        refresh_state(true);
        POINT cursor{};
        GetCursorPos(&cursor);
        HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        GetMonitorInfoW(monitor, &info);

        // Move the still-hidden window fully onto the target monitor first.
        // In a Per-Monitor-V2 process GetDpiForSystem() intentionally returns
        // 96, so it cannot be used to size a flyout on a 125%/150% display.
        // Moving first lets Windows update the window's actual monitor DPI.
        RECT current{};
        GetWindowRect(hwnd_, &current);
        const int current_width = std::max(1L, current.right - current.left);
        const int current_height = std::max(1L, current.bottom - current.top);
        const int probe_x = info.rcWork.left +
                            ((info.rcWork.right - info.rcWork.left) - current_width) / 2;
        const int probe_y = info.rcWork.top +
                            ((info.rcWork.bottom - info.rcWork.top) - current_height) / 2;
        SetWindowPos(hwnd_, nullptr, probe_x, probe_y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        dpi_ = std::max(96u, GetDpiForWindow(hwnd_));

        const int width = scale(kLogicalWidth);
        const int height = scale(kLogicalHeight);
        const int x = info.rcWork.right - width - scale(12);
        const int y = info.rcWork.bottom - height - scale(12);
        resting_x_=x;resting_y_=y;
        const bool animate=animations_enabled();
        const int initial_y=animate?y+scale(72):y;
        // Size and render while hidden. Only expose the HWND after DComp has
        // accepted a complete surface, including its 80% background tint.
        SetWindowPos(hwnd_,nullptr,x,initial_y,width,height,SWP_NOACTIVATE|SWP_NOZORDER);
        apply_backdrop();
        restore_backdrop_pending_=false;
        if(!draw_frame() || !renderer_.wait_for_first_frame()) {
            renderer_.discard_device_resources();
            show_balloon(chinese()?L"无法绘制面板，请重试。":L"Could not draw the flyout. Please try again.",NIIF_WARNING);
            return;
        }
        DwmFlush();
        trace_event("show-ready",acrylic_);
        visible_ = true;
        SetWindowPos(hwnd_, HWND_TOPMOST, x, initial_y, width, height, SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd_);
        SetFocus(hwnd_);
        // Keep the proven window-level Acrylic activation refresh, now after
        // the full content already exists, not via a delayed empty first paint.
        apply_backdrop();
        ValidateRect(hwnd_,nullptr);
        if(visible_ && !closing_ && animate) start_motion(initial_y,y,false);
    }

    void hide_flyout() {
        if(!visible_ || closing_) return;
        if(animations_enabled()) {
            RECT rect{};GetWindowRect(hwnd_,&rect);
            start_motion(rect.top,resting_y_+scale(32),true);
        } else finish_hide();
    }

    bool animations_enabled() const noexcept {
        BOOL enabled=TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&enabled,0);
        return enabled!=FALSE;
    }

    void start_motion(int from,int to,bool closing) {
        cancel_motion();
        closing_=closing;motion_to_=to;
        const bool scheduled=motion_.start(from,to,closing);
        trace_event("motion-scheduled",scheduled,closing);
        motion_active_=scheduled;
        if(!scheduled || !PostMessageW(hwnd_,kMotionFrameMessage,motion_generation_,0)) {
            motion_active_=false;
            motion_.cancel();
            if(closing) finish_hide();
            else SetWindowPos(hwnd_,nullptr,resting_x_,resting_y_,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        }
    }

    void advance_motion() {
        int y=motion_to_;bool finished=false;
        if(!motion_.sample(y,finished)) finished=true;
        SetWindowPos(hwnd_,nullptr,resting_x_,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        trace_event("motion-frame",y,finished);
        if(finished) {
            cancel_motion();
            trace_event("motion-end",closing_,y);
            if(closing_) finish_hide();
        } else {
            // Pace HWND movement against monitor vblank, not WM_TIMER. There is
            // only one outstanding frame message, tagged to reject stale runs.
            UpdateWindow(hwnd_);
            if(!renderer_.wait_for_animation_frame() || !PostMessageW(hwnd_,kMotionFrameMessage,motion_generation_,0)) {
                cancel_motion();
                if(closing_) finish_hide();
                else SetWindowPos(hwnd_,nullptr,resting_x_,resting_y_,0,0,
                    SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            }
        }
    }

    void cancel_motion() noexcept {
        motion_active_=false;
        ++motion_generation_;
        motion_.cancel();
    }

    void finish_hide() {
        cancel_motion();
        closing_=false;
        trace_event("hide",visible_,acrylic_);
        visible_ = false;
        restore_backdrop_pending_=false;
        ShowWindow(hwnd_, SW_HIDE);
        renderer_.discard_device_resources();
        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    }

    void cleanup() {
        KillTimer(hwnd_, kRefreshTimer);
        cancel_motion();
        motion_.shutdown();
        clear_keep_awake();
        renderer_.discard_device_resources();
        if (tray_added_) Shell_NotifyIconW(NIM_DELETE, &tray_);
        tray_added_ = false;
        if (tray_icon_) DestroyIcon(tray_icon_);
        tray_icon_ = nullptr;
        if (mutex_) CloseHandle(mutex_);
        mutex_ = nullptr;
    }

    HINSTANCE instance_{};
    bool chinese() const noexcept {
        return language_==1 || (language_==0 && PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_CHINESE);
    }
    void set_language(DWORD value) {
        language_=value;
        RegSetKeyValueW(HKEY_CURRENT_USER,kPreferences,L"Language",REG_DWORD,&language_,sizeof(language_));
        update_tray_icon(); InvalidateRect(hwnd_,nullptr,FALSE);
    }
    DWORD language_{};
    int hover_{-1};
    HWND hwnd_{};
    HANDLE mutex_{};
    NOTIFYICONDATAW tray_{};
    HICON tray_icon_{};
    UINT taskbar_created_message_{};
    bool tray_added_{};
    TrayClick tray_click_{};
    UINT dpi_{96};
    bool light_theme_{};
    bool acrylic_{};
    bool visible_{};
    bool closing_{};
    int resting_x_{},resting_y_{},motion_to_{};
    NativeMotion motion_{};
    bool motion_active_{};
    WPARAM motion_generation_{};
    bool restore_backdrop_pending_{};
    bool preview_{};
    bool menu_open_{};
    bool keep_awake_{};
    bool awake_release_pending_{};
    bool awake_warning_shown_{};
    bool dragging_{};
    bool dragging_battery_{};
    int drag_position_{};
    int ac_position_{1};
    int battery_position_{1};
    bool energy_saver_active_{};
    SupplyKind supply_{SupplyKind::Unknown};
    PowerController power_{};
    Renderer renderer_{};
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    App app;
    return app.run(instance, show_command);
}
