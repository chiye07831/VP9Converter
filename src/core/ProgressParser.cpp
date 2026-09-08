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

double parseDurationLine(const std::string& output)
{
    size_t dpos = output.find("Duration: ");
    if (dpos == std::string::npos)
        return 0.0;
    const char* p = output.c_str() + dpos + 10;
    if (strncmp(p, "N/A", 3) == 0)
        return 0.0;
    int h = 0, m = 0;
    double s = 0.0;
    if (sscanf(p, "%d:%d:%lf", &h, &m, &s) >= 2)
        return h * 3600.0 + m * 60.0 + s;
    return 0.0;
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

    double dur = std::atof(output.c_str());
    if (dur > 0.0)
        return dur;

    std::vector<std::string> args2;
    args2.push_back("ffprobe");
    args2.push_back("-v");
    args2.push_back("error");
    args2.push_back("-select_streams");
    args2.push_back("v:0");
    args2.push_back("-show_entries");
    args2.push_back("stream=duration");
    args2.push_back("-of");
    args2.push_back("default=noprint_wrappers=1:nokey=1");
    args2.push_back(inputPath);

    std::string output2;
    if (!ProcessRunner::runAndWait(args2, output2))
        return 0.0;

    return std::atof(output2.c_str());
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
