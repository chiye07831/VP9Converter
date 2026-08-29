#include "FFmpegDetector.h"
#include "core/ProcessRunner.h"
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#endif

namespace FFmpegDetector {

#ifdef _WIN32

bool checkFFmpeg()
{
    WCHAR buf[MAX_PATH];
    return SearchPathW(nullptr, L"ffmpeg", L".exe", MAX_PATH, buf, nullptr) > 0;
}

bool checkFFprobe()
{
    WCHAR buf[MAX_PATH];
    return SearchPathW(nullptr, L"ffprobe", L".exe", MAX_PATH, buf, nullptr) > 0;
}

bool checkWannaCRI()
{
    std::vector<std::string> args = { "pip", "show", "WannaCRI" };
    std::string output;
    if (!ProcessRunner::runAndWait(args, output))
        return false;
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
