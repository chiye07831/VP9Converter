#include "FileDialog.h"

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

static const char* MEDIA_FILTER =
    "Media Files\0*.mp4;*.mkv;*.avi;*.mov;*.flv;*.webm;*.avc;*.wmv;*.m4v;"
    "*.mp3;*.wav;*.flac;*.ogg;*.wma;*.aac;*.m4a\0"
    "All Files\0*.*\0";

std::string openFile()
{
    OPENFILENAMEA ofn = {};
    char buf[1024] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = MEDIA_FILTER;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof(buf);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetOpenFileNameA(&ofn))
        return buf;
    return {};
}

std::vector<std::string> openFiles()
{
    OPENFILENAMEA ofn = {};
    char buf[8192] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = MEDIA_FILTER;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof(buf);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    if (!GetOpenFileNameA(&ofn))
        return {};

    std::vector<std::string> files;
    if (buf[ofn.nFileOffset] == '\0')
    {
        files.push_back(buf);
        return files;
    }

    std::string dir = std::string(buf, ofn.nFileOffset - 1);
    if (!dir.empty() && dir.back() != '\\' && dir.back() != '/')
        dir += '\\';

    const char* p = buf + ofn.nFileOffset;
    while (*p)
    {
        std::string name(p);
        files.push_back(dir + name);
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
                    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0)
                    {
                        char* buf = new char[len];
                        WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, len, nullptr, nullptr);
                        result = buf;
                        delete[] buf;
                    }
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
