#include "native_motion.h"
#include "tray_icon.h"
#include "tray_click.h"
#include "awake_safety.h"
#include <cstdio>
#include <set>
#include <cassert>

int main() {
    SYSTEM_POWER_STATUS battery{};battery.ACLineStatus=0;
    battery.BatteryLifePercent=6;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::None);
    battery.BatteryLifePercent=5;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::LowBattery);
    battery.BatteryLifePercent=0;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::LowBattery);
    battery.BatteryLifePercent=12;
    assert(awake_stop_reason(true,battery,10)==AwakeStopReason::LowBattery);
    battery.BatteryLifePercent=80;battery.BatteryFlag=4;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::LowBattery);
    battery.ACLineStatus=1;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::None);
    battery.ACLineStatus=0;battery.BatteryFlag=255;battery.BatteryLifePercent=255;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::UnknownPower);
    battery.BatteryFlag=128;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::None);
    assert(awake_stop_reason(false,battery,2)==AwakeStopReason::UnknownPower);
    battery.BatteryFlag=0;battery.BatteryLifePercent=80;battery.ACLineStatus=255;
    assert(awake_stop_reason(true,battery,2)==AwakeStopReason::UnknownPower);
    assert(awake_release_threshold(100)==100);
    std::puts("Keep Awake battery safety boundaries and telemetry failures passed.");
    TrayClick clicks;
    clicks.down(false,10);assert(!clicks.up(false,20)); // Closed -> open.
    clicks.down(true,30);assert(clicks.up(false,2000)); // Long press, already hidden.
    clicks.focus_lost_on_icon(3000);clicks.down(false,3100);
    assert(clicks.up(false,5000)); // Explorer activation precedes down.
    clicks.down(false,5100);assert(!clicks.up(false,5200)); // Next click opens.
    clicks.focus_lost_on_icon(6000);clicks.down(false,8000);
    assert(!clicks.up(false,8010)); // Abandoned stale gesture does not eat a click.
    std::puts("Tray click focus-loss and long-press cases passed.");
    HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(hr)) return 1;
    NativeMotion motion;
    auto exercise=[&](bool closing) {
        assert(motion.start(0,100,closing));
        int previous=0,value=0,early=-1,middle=-1;bool done=false;
        ULONGLONG start=GetTickCount64();
        double duration=closing?167:250;
        while(!done && GetTickCount64()-start<1000) {
            assert(motion.sample(value,done));
            assert(value>=previous && value<=100);
            double fraction=(GetTickCount64()-start)/duration;
            if(early<0 && fraction>=.25) early=value;
            if(middle<0 && fraction>=.5) middle=value;
            previous=value;Sleep(5);
        }
        assert(done && value==100);
        assert(GetTickCount64()-start<500); // Must not rely on the 1-second fail-safe.
        assert(closing?middle<30:middle>82); // Fluent Bezier, not quadratic easing.
        std::printf("%s: quarter=%d midpoint=%d final=%d\n",closing?"Exit":"Enter",early,middle,value);
    };
    exercise(false);exercise(true);
    assert(motion.start(100,0,false));Sleep(40);
    int current=100;bool done=false;assert(motion.sample(current,done));
    assert(motion.start(current,100,true));
    Sleep(200);assert(motion.sample(current,done));assert(done && current==100);
    motion.shutdown();

    std::set<std::uint64_t> hashes;
    for(int mode=0;mode<4;++mode) {
        HICON icon=make_mode_tray_icon(mode,false);assert(icon);
        ICONINFO ii{};assert(GetIconInfo(icon,&ii));
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth=32;bi.bmiHeader.biHeight=-32;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
        std::uint32_t pixels[1024]{};HDC dc=GetDC(nullptr);
        assert(GetDIBits(dc,ii.hbmColor,0,32,pixels,&bi,DIB_RGB_COLORS)==32);
        ReleaseDC(nullptr,dc);
        std::uint64_t hash=1469598103934665603ULL;unsigned coverage=0;
        for(auto pixel:pixels){hash=(hash^pixel)*1099511628211ULL;if(pixel>>24)++coverage;}
        assert(coverage>20 && coverage<800);assert(hashes.insert(hash).second);
        std::printf("Mode %d: %u nontransparent pixels, distinct glyph\n",mode,coverage);
        DeleteObject(ii.hbmColor);DeleteObject(ii.hbmMask);DestroyIcon(icon);
    }
    for(const auto& artwork:kTrayArtwork) for(int mode=0;mode<4;++mode) {
        HICON icon=make_mode_tray_icon(mode,true,artwork.size);assert(icon);
        ICONINFO ii{};assert(GetIconInfo(icon,&ii));
        BITMAP bitmap{};assert(GetObjectW(ii.hbmColor,sizeof(bitmap),&bitmap));
        assert(bitmap.bmWidth==artwork.size && bitmap.bmHeight==artwork.size);
        int left=artwork.size,top=artwork.size,right=-1,bottom=-1;
        for(int y=0;y<artwork.size;++y) for(int x=0;x<artwork.size;++x) {
            if(artwork.modes[mode][y*artwork.size+x]>32) {
                left=std::min(left,x);right=std::max(right,x);
                top=std::min(top,y);bottom=std::max(bottom,y);
            }
        }
        assert(left>0 && top>0 && right<artwork.size-1 && bottom<artwork.size-1);
        assert(std::max(right-left+1,bottom-top+1)>=artwork.size*0.8);
        assert(std::abs(left+right+1-artwork.size)<=2);
        assert(std::abs(top+bottom+1-artwork.size)<=2);
        DeleteObject(ii.hbmColor);DeleteObject(ii.hbmMask);DestroyIcon(icon);
    }
    std::printf("8 DPI sizes: bounds, centering and HICON dimensions passed. Tray requests %dpx.\n",taskbar_icon_size());
    const DWORD gdi_before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    const DWORD user_before=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
    for(int i=0;i<128;++i) {
        HICON icon=make_mode_tray_icon(i%4,true);assert(icon);DestroyIcon(icon);
    }
    assert(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==gdi_before);
    assert(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)==user_before);
    CoUninitialize();
    std::puts("Native animation, interruption, 4 tray glyphs and handle cleanup passed.");
}
