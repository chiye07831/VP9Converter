#include "FileDialog.h"
#include "WinConv.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#else
#include <cstdio>
#endif

#include <algorithm>

namespace FileDialog {

#ifdef _WIN32

static const WCHAR* MEDIA_FILTER_W =
    L"Media Files\0*.mp4;*.mkv;*.avi;*.mov;*.flv;*.webm;*.avc;*.wmv;*.m4v;"
    L"*.mp3;*.wav;*.flac;*.ogg;*.wma;*.aac;*.m4a\0"
    L"All Files\0*.*\0";

std::string openFile()
{
    OPENFILENAMEW ofn = {};
    WCHAR buf[1024] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = MEDIA_FILTER_W;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 1024;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetOpenFileNameW(&ofn))
        return WinConv::wideToUtf8(buf);
    return {};
}

std::vector<std::string> openFiles()
{
    OPENFILENAMEW ofn = {};
    WCHAR buf[8192] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = MEDIA_FILTER_W;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 8192;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    if (!GetOpenFileNameW(&ofn))
        return {};

    std::vector<std::string> files;
    if (buf[ofn.nFileOffset] == L'\0')
    {
        files.push_back(WinConv::wideToUtf8(buf));
        return files;
    }

    std::wstring dir(buf, ofn.nFileOffset - 1);
    if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/')
        dir += L'\\';

    const WCHAR* p = buf + ofn.nFileOffset;
    while (*p)
    {
        std::wstring name(p);
        files.push_back(WinConv::wideToUtf8(dir + name));
        p += name.size() + 1;
    }
    return files;
}

std::string openFolder()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IFileOpenDialog* pfd = nullptr;
    std::string result;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr))
    {
        DWORD flags;
        pfd->GetOptions(&flags);
        pfd->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        pfd->SetTitle(L"Select Output Folder");

        if (SUCCEEDED(pfd->Show(nullptr)))
        {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    result = WinConv::wideToUtf8(path);
                    CoTaskMemFree(path);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    return result;
}

#else

std::string openFile()
{
    FILE* pipe = popen(
        "zenity --file-selection "
        "--file-filter=\"Media files | *.mp4 *.mkv *.avi *.mov *.flv *.webm *.avc *.wmv *.m4v "
        "*.mp3 *.wav *.flac *.ogg *.wma *.aac *.m4a\" "
        "2>/dev/null",
        "r");
    if (!pipe) return {};
    char buf[1024];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe))
        result += buf;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

std::vector<std::string> openFiles()
{
    std::string path = openFile();
    if (path.empty())
        return {};
    return { path };
}

std::string openFolder()
{
    FILE* pipe = popen(
        "zenity --file-selection --directory 2>/dev/null", "r");
    if (!pipe) return {};
    char buf[1024];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe))
        result += buf;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

#endif

std::string getParentDir(const std::string& path)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
        return {};
    return path.substr(0, pos);
}

std::string getStem(const std::string& path)
{
    size_t pos = path.find_last_of("/\\");
    std::string filename = (pos == std::string::npos) ? path : path.substr(pos + 1);
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos)
        return filename;
    return filename.substr(0, dot);
}

std::string buildOutputName(const std::string& inputPath)
{
    return getStem(inputPath);
}

}
