// Integration driver for this application's own message protocol only.
// Usage: test-flyout.exe <expected-process-id> exit|cycles
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

int main(int argc,char** argv) {
    if(argc!=3) return 2;
    const DWORD expected=std::strtoul(argv[1],nullptr,10);
    HWND window=FindWindowW(L"PowerModeNative.Flyout",nullptr);
    DWORD actual=0;GetWindowThreadProcessId(window,&actual);
    if(!window || actual!=expected) return 3;
    auto send=[&](UINT message,WPARAM value=0,LPARAM parameter=0) {
        DWORD_PTR result=0;
        return SendMessageTimeoutW(window,message,value,parameter,SMTO_ABORTIFHUNG,5000,&result)!=0;
    };
    if(std::strcmp(argv[2],"exit")==0) {
        HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,expected);
        if(!process) return 4;
        const bool sent=PostMessageW(window,WM_COMMAND,1002,0)!=0;
        const bool exited=sent && WaitForSingleObject(process,5000)==WAIT_OBJECT_0;
        CloseHandle(process);return exited?0:5;
    }
    if(std::strcmp(argv[2],"cycles")!=0) return 2;
    // A synthetic WM_APP message does not carry a user's foreground grant.
    // Model an explicit launcher/tray interaction rather than a background pop-up.
    if(!AllowSetForegroundWindow(expected)) {
        std::printf("Cannot grant foreground permission for interactive test: %lu\n",GetLastError());
        return 18;
    }
    for(int i=0;i<3;++i) {
        if(!send(WM_CLOSE)) return 6;
        Sleep(400);
        if(IsWindowVisible(window)) return 7;
        if(!send(WM_APP+2)) return 8;
        Sleep(500);
        if(!IsWindowVisible(window)) return 9;
        std::printf("Flyout cycle %d: hidden then visible\n",i+1);
    }
    // Reverse an in-flight exit, then verify that stale frames cannot hide it.
    if(!send(WM_CLOSE)) return 10;
    Sleep(40);
    if(!send(WM_APP+2)) return 11;
    Sleep(500);
    if(!IsWindowVisible(window)) return 12;
    std::puts("Flyout reversal remained visible.");
    // Reproduce Explorer sending deactivate between tray down and tray up.
    for(DWORD hold:{40UL,400UL}) {
        if(!send(WM_APP+1,0,WM_LBUTTONDOWN) || !send(WM_ACTIVATE,WA_INACTIVE)) return 13;
        Sleep(hold);
        if(!send(WM_APP+1,0,WM_LBUTTONUP)) return 14;
        Sleep(400);
        if(IsWindowVisible(window)) return 15;
        if(!send(WM_APP+1,0,WM_LBUTTONDOWN) || !send(WM_APP+1,0,WM_LBUTTONUP)) return 16;
        Sleep(500);
        if(!IsWindowVisible(window)) return 17;
    }
    std::puts("Tray click closed without reopening, including release after hide.");
    return 0;
}
