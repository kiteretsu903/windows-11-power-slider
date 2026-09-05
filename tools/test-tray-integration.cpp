// Exercise the real App methods on a separate hidden window. App::run is never
// called: this test does not register startup or change the user's power mode.
#include "tray_icon.h"
#include <shellapi.h>
#include <cassert>
#include <cstdio>
static bool fail_generation=false,fail_fallback=false,fail_shell=false;
static unsigned generation_calls=0,null_submissions=0;
static HICON test_generate(int mode,bool awake,int size) noexcept {
    ++generation_calls;
    return fail_generation?nullptr:make_mode_tray_icon(mode,awake,size);
}
static HICON WINAPI test_load_icon(HINSTANCE instance,LPCWSTR name) {
    return fail_fallback?nullptr:LoadIconW(instance,name);
}
static BOOL WINAPI test_notify(DWORD op,NOTIFYICONDATAW* data) {
    if((op==NIM_ADD || op==NIM_MODIFY) && (data->uFlags&NIF_ICON) && !data->hIcon)
        ++null_submissions;
    return fail_shell?FALSE:Shell_NotifyIconW(op,data);
}
#define make_mode_tray_icon test_generate
#define LoadIconW test_load_icon
#define Shell_NotifyIconW test_notify
#include "../src/main.cpp"
#undef make_mode_tray_icon
#undef LoadIconW
#undef Shell_NotifyIconW

namespace {
struct TrayIntegrationTest {
    static bool present(App& app) {
        NOTIFYICONIDENTIFIER id{};id.cbSize=sizeof(id);id.hWnd=app.hwnd_;id.uID=1;
        RECT rect{};return SUCCEEDED(Shell_NotifyIconGetRect(&id,&rect));
    }
    static void erase(App& app) { Shell_NotifyIconW(NIM_DELETE,&app.tray_); }
    static void tick(App& app) { app.handle_message(WM_TIMER,kRefreshTimer,0); }
    static void run() {
        App app;app.instance_=GetModuleHandleW(nullptr);
        app.taskbar_created_message_=RegisterWindowMessageW(L"TaskbarCreated");
        WNDCLASSW wc{};wc.lpfnWndProc=App::window_proc;wc.hInstance=app.instance_;
        wc.lpszClassName=L"PowerSlider.TrayIntegrationTest";
        assert(RegisterClassW(&wc));
        HWND window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_NOREDIRECTIONBITMAP,
            wc.lpszClassName,L"Tray test",WS_POPUP,0,0,392,370,nullptr,nullptr,app.instance_,&app);
        assert(window);
        app.create_tray_icon();app.refresh_state(true);
        assert(app.tray_added_ && present(app));
        std::puts("Initial hidden-window registration passed.");

        erase(app);assert(!present(app));
        tick(app);assert(app.tray_added_ && present(app));
        std::puts("Lost registration recovered on the first timer tick.");
        const HICON previous=app.tray_image_.get();
        fail_generation=true;
        app.update_tray_icon();
        assert(app.tray_image_.get()==previous && app.tray_image_.pending() && present(app));
        fail_generation=false;tick(app);assert(!app.tray_image_.pending());
        std::puts("Failed refresh preserved the last good image and retried.");

        for(int i=0;i<3;++i) app.handle_message(app.taskbar_created_message_,0,0);
        assert(app.tray_added_ && present(app));
        erase(app);app.handle_message(app.taskbar_created_message_,0,0);
        assert(app.tray_added_ && present(app));
        std::puts("Duplicate and genuine TaskbarCreated recovery passed.");

        erase(app);app.tray_image_.reset();fail_generation=true;
        app.create_tray_icon();app.refresh_state(true);
        assert(app.tray_added_ && app.tray_image_.get() && app.tray_image_.pending() && present(app));
        const unsigned before=generation_calls;
        fail_generation=false;tick(app);
        assert(generation_calls>before && !app.tray_image_.pending() && present(app));
        std::puts("Startup generation failure used a resource fallback then recovered the mode icon.");

        erase(app);app.tray_image_.reset();fail_generation=true;fail_fallback=true;
        app.create_tray_icon();assert(!app.tray_added_ && !present(app));
        tick(app);assert(!app.tray_added_ && !app.tray_image_.get());
        fail_generation=false;fail_fallback=false;tick(app);
        assert(app.tray_added_ && present(app) && null_submissions==0);
        std::puts("Complete image failure retried without submitting a null icon.");

        erase(app);fail_shell=true;tick(app);assert(!app.tray_added_);
        fail_shell=false;tick(app);assert(app.tray_added_ && present(app));
        std::puts("Unavailable Shell recovered on the first successful tick.");
        const DWORD handles=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
        for(int i=0;i<50;++i) {erase(app);app.handle_message(app.taskbar_created_message_,0,0);}
        assert(app.tray_added_ && present(app));
        assert(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)==handles);
        DestroyWindow(window);
        assert(!present(app));
        UnregisterClassW(wc.lpszClassName,app.instance_);
        std::puts("Repeated recovery and cleanup passed.");
    }
};
}
int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    TrayIntegrationTest::run();
    CoUninitialize();
}
