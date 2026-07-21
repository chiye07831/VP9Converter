#pragma once

#include <string>
#include <vector>
#include "model/Task.h"

namespace FFmpegBuilder {

std::vector<std::string> buildVideoArgs(const Task& task);
std::vector<std::string> buildAudioArgs(const Task& task);
std::vector<std::string> buildVmafArgs(const Task& task);
std::vector<std::string> buildSsimArgs(const Task& task);
std::vector<std::string> buildPsnrArgs(const Task& task);
std::vector<std::string> buildUsmArgs(const Task& task);
std::string commandString(const std::vector<std::string>& args);
void detectMediaInfo(Task& task);
int getCpuThreads();

}
