#pragma once
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

namespace sea_g2p {

inline std::string utf16_to_utf8(const std::wstring& utf16) {
    if (utf16.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), (int)utf16.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), (int)utf16.size(), &utf8[0], size, nullptr, nullptr);
    return utf8;
}

inline void setup_utf8_console() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
}

inline std::vector<std::string> get_utf8_args() {
    int argc;
    LPWSTR* argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv_w) return {};
    
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(utf16_to_utf8(argv_w[i]));
    }
    LocalFree(argv_w);
    return args;
}

} // namespace sea_g2p

#else

namespace sea_g2p {

inline void setup_utf8_console() {}

inline std::vector<std::string> get_utf8_args(int argc, char* argv[]) {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return args;
}

} // namespace sea_g2p

#endif
