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

    struct CommandLogEntry
    {
        std::string time;
        std::string taskName;
        std::string phase;
        std::string command;
    };

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
    void startAudioCopy(Task* task);
    static std::string checkTaskErrors(const Task* task);
    void logCommand(const std::string& phase, const Task& task,
                    const std::vector<std::string>& args);

    bool m_ffmpegAvailable;
    bool m_wannacriAvailable;
    bool m_ffmpegWarningShown;
    bool m_wannacriWarningShown;
    bool m_sequentialQueue;

    TaskManager m_taskManager;

    std::map<int, std::unique_ptr<ProcessRunner>> m_runners;
    std::map<int, std::unique_ptr<ProcessRunner>> m_qualityRunners;
    std::map<int, std::unique_ptr<ProcessRunner>> m_frameCounters;
    std::map<int, std::chrono::steady_clock::time_point> m_videoStartTimes;

    std::vector<CommandLogEntry> m_commandLog;
    size_t m_commandLogRendered = 0;

    std::unique_ptr<ProcessRunner> m_audioCopyRunner;
    std::string m_audioCopyOutput;
    std::string m_audioCopyStatus;
    bool m_audioCopyRunning = false;

    char m_inputPathBuf[512];
    char m_outputFolderBuf[512];
    char m_outputNameBuf[256];
};
