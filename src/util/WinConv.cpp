#include "WinConv.h"

#ifdef _WIN32

namespace WinConv {

std::wstring utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                  utf8.c_str(), static_cast<int>(utf8.size()),
                                  nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0,
                        utf8.c_str(), static_cast<int>(utf8.size()),
                        &wide[0], len);
    return wide;
}

std::string wideToUtf8(const std::wstring& wide)
{
    if (wide.empty())
        return {};
    int len = WideCharToMultiByte(CP_UTF8, 0,
                                  wide.c_str(), static_cast<int>(wide.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string utf8(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0,
                        wide.c_str(), static_cast<int>(wide.size()),
                        &utf8[0], len, nullptr, nullptr);
    return utf8;
}

std::wstring quoteWindowsArg(const std::wstring& arg)
{
    if (arg.empty())
        return L"\"\"";

    if (arg.find_first_of(L" \t\"") == std::wstring::npos)
        return arg;

    std::wstring out;
    out += L'"';
    size_t backslashes = 0;
    for (wchar_t c : arg)
    {
        if (c == L'\\')
        {
            ++backslashes;
        }
        else if (c == L'"')
        {
            out.append(backslashes * 2, L'\\');
            backslashes = 0;
            out += L"\\\"";
        }
        else
        {
            out.append(backslashes, L'\\');
            backslashes = 0;
            out += c;
        }
    }
    out.append(backslashes * 2, L'\\');
    out += L'"';
    return out;
}

std::wstring buildCommandLine(const std::vector<std::string>& args)
{
    std::wstring cmd;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0)
            cmd += L' ';
        cmd += quoteWindowsArg(utf8ToWide(args[i]));
    }
    return cmd;
}

}

#endif
