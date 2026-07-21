#pragma once

#include <string>
#include <vector>

namespace FileDialog {

std::string openFile();
std::vector<std::string> openFiles();
std::string openFolder();

std::string getParentDir(const std::string& path);
std::string getStem(const std::string& path);
std::string buildOutputName(const std::string& inputPath);

}
