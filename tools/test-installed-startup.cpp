// Exercise duplicate --startup launches against one explicitly named process.
// Does not change power modes, startup preferences, or unrelated processes.
#include <windows.h>
#include <string>
#include <cstdio>
#include <cwchar>

int wmain(int argc,wchar_t** argv) {
    if(argc!=4) return 2; // expected PID, installed EXE, old|fixed
    const DWORD expected=wcstoul(argv[1],nullptr,10);
    HWND window=FindWindowW(L"PowerModeNative.Flyout",nullptr);
    DWORD actual=0;GetWindowThreadProcessId(window,&actual);
    if(!window || actual!=expected) return 3;
    const bool expect_old=wcscmp(argv[3],L"old")==0;
    PostMessageW(window,WM_CLOSE,0,0);Sleep(600);
    if(IsWindowVisible(window)) return 4;
    for(int i=0;i<3;++i) {
        std::wstring command=L"\""+std::wstring(argv[2])+L"\" --startup";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);
        PROCESS_INFORMATION process{};
        if(!CreateProcessW(argv[2],command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)) return 5;
        const DWORD result=WaitForSingleObject(process.hProcess,5000);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        if(result!=WAIT_OBJECT_0) return 6;
        Sleep(500);
        const bool visible=IsWindowVisible(window)!=FALSE;
        std::printf("Duplicate startup %d: flyout %s\n",i+1,visible?"OPEN":"hidden");
        PostMessageW(window,WM_CLOSE,0,0);Sleep(500);
        if(visible!=expect_old) return 7;
    }
    std::puts(expect_old?"Reproduced the old unsolicited-popup bug.":"Repeated startup stayed silent.");
}
