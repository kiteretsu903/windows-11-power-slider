#pragma once
#include <cwchar>

struct LaunchOptions {
    bool startup{}, show{}, preview{}, shutdown{};
    static LaunchOptions parse(int count, const wchar_t* const* arguments) noexcept {
        LaunchOptions result;
        // Skip argv[0]: a directory name must not become a command-line flag.
        for (int i=1; i<count; ++i) {
            if (!arguments[i]) continue;
            if (std::wcscmp(arguments[i],L"--startup")==0) result.startup=true;
            else if (std::wcscmp(arguments[i],L"--show")==0) result.show=true;
            else if (std::wcscmp(arguments[i],L"--preview")==0) result.preview=true;
            else if (std::wcscmp(arguments[i],L"--shutdown")==0) result.shutdown=true;
        }
        return result;
    }
    bool show_existing() const noexcept { return !startup && !shutdown; }
    bool show_new() const noexcept { return !startup && !shutdown && (show || preview); }
};
