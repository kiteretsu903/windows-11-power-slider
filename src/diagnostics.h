#pragma once
#include <windows.h>
#include <cstdio>

// Opt-in diagnostics: no file is opened unless a caller supplies this path.
inline void trace_event(const char* event, long long a=0, long long b=0) noexcept {
    wchar_t path[32768]{};
    if(!GetEnvironmentVariableW(L"PMN_TRACE_FILE",path,32768)) return;
    HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE) return;
    char line[200]{};
    int n=std::snprintf(line,sizeof(line),"%llu %s %lld %lld\r\n",GetTickCount64(),event,a,b);
    DWORD written=0; if(n>0) WriteFile(f,line,DWORD(n),&written,nullptr);
    CloseHandle(f);
}
