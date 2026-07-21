#pragma once

#include <string>

struct ProgressInfo
{
    double currentTime = 0.0;
    double speed = 0.0;
    int frame = 0;
};

namespace ProgressParser {

void parseOutput(const std::string& stderrOutput, ProgressInfo& info);
double getDuration(const std::string& inputPath);

}
