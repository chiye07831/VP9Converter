#include "ProgressParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#endif

namespace ProgressParser {

void parseOutput(const std::string& stderrOutput, ProgressInfo& info)
{
    info.currentTime = 0.0;
    info.speed = 0.0;
    info.frame = 0;

    size_t timePos = stderrOutput.rfind("time=");
    if (timePos != std::string::npos)
    {
        const char* p = stderrOutput.c_str() + timePos + 5;
        int h = 0, m = 0;
        double s = 0.0;
        if (sscanf(p, "%d:%d:%lf", &h, &m, &s) >= 3)
            info.currentTime = h * 3600.0 + m * 60.0 + s;
    }

    size_t speedPos = stderrOutput.rfind("speed=");
    if (speedPos != std::string::npos)
    {
        const char* p = stderrOutput.c_str() + speedPos + 6;
        double val = 0.0;
        if (sscanf(p, "%lf", &val) >= 1)
            info.speed = val;
    }

    size_t framePos = stderrOutput.rfind("frame=");
    if (framePos != std::string::npos)
    {
        const char* p = stderrOutput.c_str() + framePos + 6;
        int val = 0;
        if (sscanf(p, "%d", &val) >= 1)
            info.frame = val;
    }
}

#ifdef _WIN32

double getDuration(const std::string& inputPath)
{
    std::string cmd = "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"";
    cmd += inputPath;
    cmd += "\"";

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return 0.0;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return 0.0;
    }

    CloseHandle(hWrite);
    CloseHandle(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    char buf[128] = {};
    DWORD totalRead = 0;
    DWORD bytesRead;
    while (ReadFile(hRead, buf + totalRead, sizeof(buf) - totalRead - 1, &bytesRead, nullptr) && bytesRead > 0)
        totalRead += bytesRead;
    buf[totalRead] = '\0';
    CloseHandle(hRead);

    double duration = 0.0;
    sscanf(buf, "%lf", &duration);
    return duration;
}

#else

double getDuration(const std::string& inputPath)
{
    std::string cmd = "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"";
    cmd += inputPath;
    cmd += "\" 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return 0.0;

    char buf[128] = {};
    if (fgets(buf, sizeof(buf), pipe))
    {
        pclose(pipe);
        double duration = 0.0;
        sscanf(buf, "%lf", &duration);
        return duration;
    }
    pclose(pipe);
    return 0.0;
}

#endif

}
