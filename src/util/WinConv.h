#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace WinConv {

#ifdef _WIN32

std::wstring utf8ToWide(const std::string& utf8);

std::string wideToUtf8(const std::wstring& wide);

std::wstring quoteWindowsArg(const std::wstring& arg);

std::wstring buildCommandLine(const std::vector<std::string>& args);

#endif

}
