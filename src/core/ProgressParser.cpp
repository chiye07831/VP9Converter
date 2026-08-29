#include "ProgressParser.h"
#include "core/ProcessRunner.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    std::vector<std::string> args;
    args.push_back("ffprobe");
    args.push_back("-v");
    args.push_back("error");
    args.push_back("-show_entries");
    args.push_back("format=duration");
    args.push_back("-of");
    args.push_back("default=noprint_wrappers=1:nokey=1");
    args.push_back(inputPath);

    std::string output;
    if (!ProcessRunner::runAndWait(args, output))
        return 0.0;

    return std::atof(output.c_str());
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
