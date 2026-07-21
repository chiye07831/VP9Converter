#pragma once

#include <map>
#include <memory>
#include <chrono>
#include <vector>
#include <string>
#include "model/TaskManager.h"
#include "core/ProcessRunner.h"

class MainWindow
{
public:
    MainWindow(bool ffmpegAvailable, bool wannacriAvailable);
    void render();
    void addDroppedFiles(const std::vector<std::string>& paths);

private:
    void renderLeftPanel();
    void renderRightPanel();
    void renderInputSection();
    void renderVideoSection();
    void renderAudioSection();
    void renderActionsSection();
    void renderQualitySection();
    void syncFromTask();
    void syncToTask();
    void updateRunningProcesses();
    void startEncoding(Task* task, int index);
    void startQualityCheck(Task* task, int index);
    static std::string checkTaskErrors(const Task* task);
    static std::string buildCommandPreview(const Task& task);

    bool m_ffmpegAvailable;
    bool m_wannacriAvailable;
    bool m_ffmpegWarningShown;
    bool m_wannacriWarningShown;
    bool m_sequentialQueue;

    TaskManager m_taskManager;

    std::map<int, std::unique_ptr<ProcessRunner>> m_runners;
    std::map<int, std::unique_ptr<ProcessRunner>> m_qualityRunners;
    std::map<int, std::chrono::steady_clock::time_point> m_videoStartTimes;

    char m_inputPathBuf[512];
    char m_outputFolderBuf[512];
    char m_outputNameBuf[256];
};
