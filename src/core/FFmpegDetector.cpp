#include "FFmpegDetector.h"
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#endif

namespace FFmpegDetector {

#ifdef _WIN32

bool checkFFmpeg()
{
    char buf[MAX_PATH];
    return SearchPathA(nullptr, "ffmpeg", ".exe", MAX_PATH, buf, nullptr) > 0;
}

bool checkFFprobe()
{
    char buf[MAX_PATH];
    return SearchPathA(nullptr, "ffprobe", ".exe", MAX_PATH, buf, nullptr) > 0;
}

bool checkWannaCRI()
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    std::string cmd = "pip show WannaCRI";
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    CloseHandle(hWrite);
    CloseHandle(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    char buf[512] = {};
    DWORD totalRead = 0;
    DWORD bytesRead;
    while (ReadFile(hRead, buf + totalRead, sizeof(buf) - totalRead - 1, &bytesRead, nullptr) && bytesRead > 0)
        totalRead += bytesRead;
    buf[totalRead] = '\0';
    CloseHandle(hRead);

    std::string output(buf);
    return output.find("Name: WannaCRI") != std::string::npos;
}

#else

static bool checkInPath(const char* name)
{
    FILE* pipe = popen(("which " + std::string(name) + " 2>/dev/null").c_str(), "r");
    if (!pipe) return false;
    char buf[256];
    bool found = fgets(buf, sizeof(buf), pipe) != nullptr;
    pclose(pipe);
    return found;
}

bool checkFFmpeg() { return checkInPath("ffmpeg"); }
bool checkFFprobe() { return checkInPath("ffprobe"); }

bool checkWannaCRI()
{
    FILE* pipe = popen("pip show WannaCRI 2>/dev/null", "r");
    if (!pipe) return false;
    char buf[512] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);
    return output.find("Name: WannaCRI") != std::string::npos;
}

#endif

}
