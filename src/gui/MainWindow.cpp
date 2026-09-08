#include "MainWindow.h"
#include "imgui.h"
#include "util/FileDialog.h"
#include "util/WinConv.h"
#include "core/FFmpegBuilder.h"
#include "core/ProgressParser.h"
#include <ctime>
#include <cstdlib>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

static const char* statusLabel(int s)
{
    switch (s)
    {
    case 0: return "Waiting";
    case 1: return "Running";
    case 2: return "Finished";
    case 3: return "Failed";
    case 4: return "Canceled";
    }
    return "";
}

static void formatTime(double seconds, char* buf, int size)
{
    int h = static_cast<int>(seconds) / 3600;
    int m = (static_cast<int>(seconds) % 3600) / 60;
    int s = static_cast<int>(seconds) % 60;
    snprintf(buf, size, "%02d:%02d:%02d", h, m, s);
}

static std::string formatBitrate(int64_t bps)
{
    if (bps <= 0) return "?";
    if (bps >= 1000000)
    {
        char b[32];
        snprintf(b, sizeof(b), "%.2f Mbps", bps / 1000000.0);
        return b;
    }
    if (bps >= 1000)
    {
        char b[32];
        snprintf(b, sizeof(b), "%.0f kbps", bps / 1000.0);
        return b;
    }
    return std::to_string(bps) + " bps";
}

static bool fileExists(const std::string& path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesW(WinConv::utf8ToWide(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

static bool dirExists(const std::string& path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesW(WinConv::utf8ToWide(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

std::string MainWindow::checkTaskErrors(const Task* task)
{
    if (task->inputPath.empty())
        return "Input file is not specified";
    if (!fileExists(task->inputPath))
        return "Input file does not exist: " + task->inputPath;
    if (task->outputFolder.empty())
        return "Output folder is not specified";
    if (!dirExists(task->outputFolder))
        return "Output folder does not exist: " + task->outputFolder;
    return {};
}

static std::string currentTimeString()
{
    time_t t = time(nullptr);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &tmv);
    return buf;
}

void MainWindow::logCommand(const std::string& phase, const Task& task,
                            const std::vector<std::string>& args)
{
    if (args.empty())
        return;
    CommandLogEntry entry;
    entry.time = currentTimeString();
    entry.taskName = task.outputName;
    entry.phase = phase;
    entry.command = FFmpegBuilder::commandString(args);
    m_commandLog.push_back(std::move(entry));
}

MainWindow::MainWindow(bool ffmpegAvailable, bool wannacriAvailable)
    : m_ffmpegAvailable(ffmpegAvailable)
    , m_wannacriAvailable(wannacriAvailable)
    , m_ffmpegWarningShown(false)
    , m_wannacriWarningShown(false)
    , m_sequentialQueue(false)
{
    m_inputPathBuf[0] = '\0';
    m_outputFolderBuf[0] = '\0';
    m_outputNameBuf[0] = '\0';
    syncFromTask();
}

void MainWindow::syncFromTask()
{
    const Task* task = m_taskManager.current();
    if (!task) return;
    snprintf(m_inputPathBuf, sizeof(m_inputPathBuf), "%s", task->inputPath.c_str());
    snprintf(m_outputFolderBuf, sizeof(m_outputFolderBuf), "%s", task->outputFolder.c_str());
    snprintf(m_outputNameBuf, sizeof(m_outputNameBuf), "%s", task->outputName.c_str());
}

void MainWindow::syncToTask()
{
    Task* task = m_taskManager.current();
    if (!task) return;
    task->inputPath = m_inputPathBuf;
    task->outputFolder = m_outputFolderBuf;
    task->outputName = m_outputNameBuf;
}

void MainWindow::addDroppedFiles(const std::vector<std::string>& paths)
{
    for (const auto& path : paths)
    {
        Task* t = m_taskManager.add();
        t->inputPath = path;
        t->outputFolder = FileDialog::getParentDir(path);
        t->outputName = FileDialog::buildOutputName(path);
        FFmpegBuilder::detectMediaInfo(*t);
        t->videoEnabled = t->hasVideoSource;
        t->audioEnabled = t->hasAudioSource;
    }
    if (!paths.empty())
    {
        m_taskManager.select(m_taskManager.count() - static_cast<int>(paths.size()));
        syncFromTask();
    }
}

void MainWindow::render()
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_MenuBar;

    updateRunningProcesses();

    ImGui::Begin("VP9Converter", nullptr, flags);

    if (!m_ffmpegAvailable && !m_ffmpegWarningShown)
    {
        ImGui::OpenPopup("FFmpeg Not Found");
        m_ffmpegWarningShown = true;
    }
    if (ImGui::BeginPopupModal("FFmpeg Not Found", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("FFmpeg not found in system PATH.");
        ImGui::TextUnformatted("Please add FFmpeg to PATH to enable encoding.");
        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (m_ffmpegAvailable && !m_wannacriAvailable && !m_wannacriWarningShown)
    {
        ImGui::OpenPopup("WannaCRI Not Found");
        m_wannacriWarningShown = true;
    }
    if (ImGui::BeginPopupModal("WannaCRI Not Found", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Python WannaCRI is not installed.");
        ImGui::TextUnformatted("USM conversion will be unavailable.");
        ImGui::Separator();
        if (ImGui::Button("Install (pip install WannaCRI==0.3.0)"))
        {
            system("pip install WannaCRI==0.3.0");
            m_wannacriAvailable = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Skip"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    float panelWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = panelWidth * 0.35f;
    float rightWidth = panelWidth - leftWidth;

    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, 0), false, ImGuiWindowFlags_NoScrollbar);
    renderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPanel", ImVec2(rightWidth, 0), false, ImGuiWindowFlags_NoScrollbar);
    renderRightPanel();
    ImGui::EndChild();

    ImGui::End();
}

void MainWindow::renderLeftPanel()
{
    if (!m_taskManager.current())
    {
        ImGui::Text("No task selected");
        return;
    }

    syncFromTask();
    renderInputSection();
    ImGui::Separator();
    renderVideoSection();
    ImGui::Separator();
    renderAudioSection();
    ImGui::Separator();
    renderQualitySection();
    ImGui::Separator();
    renderActionsSection();
    syncToTask();
}

void MainWindow::renderInputSection()
{
    if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Input File");
        {
            float btnW = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f;
            ImGui::SetNextItemWidth(-btnW - ImGui::GetStyle().ItemInnerSpacing.x);
        }
        ImGui::InputText("##inputPath", m_inputPathBuf, sizeof(m_inputPathBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Browse##input"))
        {
            std::string path = FileDialog::openFile();
            if (!path.empty())
            {
                Task* task = m_taskManager.current();
                if (task)
                {
                    task->inputPath = path;
                    task->outputFolder = FileDialog::getParentDir(path);
                    task->outputName = FileDialog::buildOutputName(path);
                    FFmpegBuilder::detectMediaInfo(*task);
                    task->videoEnabled = task->hasVideoSource;
                    task->audioEnabled = task->hasAudioSource;
                    syncFromTask();
                }
            }
        }

        ImGui::Text("Output Folder");
        {
            float btnW = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f;
            ImGui::SetNextItemWidth(-btnW - ImGui::GetStyle().ItemInnerSpacing.x);
        }
        ImGui::InputText("##outputFolder", m_outputFolderBuf, sizeof(m_outputFolderBuf), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Browse##output"))
        {
            std::string folder = FileDialog::openFolder();
            if (!folder.empty())
            {
                Task* task = m_taskManager.current();
                if (task)
                {
                    task->outputFolder = folder;
                    syncFromTask();
                }
            }
        }

        float centerX = ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - 220.0f) * 0.5f;
        ImGui::SetCursorPosX(centerX);
        if (ImGui::Button("Change All Task Output Folder", ImVec2(220, 0)))
        {
            Task* current = m_taskManager.current();
            if (current)
            {
                for (int i = 0; i < m_taskManager.count(); ++i)
                    m_taskManager.get(i)->outputFolder = current->outputFolder;
            }
        }

        ImGui::Separator();

        ImGui::Text("Output Name");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##outputName", m_outputNameBuf, sizeof(m_outputNameBuf));

        ImGui::Separator();
        ImGui::Text("Source Info");
        Task* task = m_taskManager.current();
        if (task && (task->hasVideoSource || task->hasAudioSource))
        {
            char dur[32];
            formatTime(task->duration, dur, sizeof(dur));
            std::string storage = (task->srcWidth > 0 && task->srcHeight > 0)
                ? std::to_string(task->srcWidth) + " x " + std::to_string(task->srcHeight)
                : "?";
            std::string display = (task->dispWidth > 0 && task->dispHeight > 0)
                ? std::to_string(task->dispWidth) + " x " + std::to_string(task->dispHeight)
                : "?";
            ImGui::Text("Storage : %s", storage.c_str());
            ImGui::Text("Display : %s", display.c_str());
            if (task->frameRate > 0.0)
                ImGui::Text("Frame   : %.3f fps", task->frameRate);
            else
                ImGui::Text("Frame   : -");
            std::string vInfo = task->hasVideoSource
                ? (task->videoCodec.empty() ? "?" : task->videoCodec) : "-";
            if (task->hasVideoSource && task->videoBitrate > 0)
                vInfo += "  (" + formatBitrate(task->videoBitrate) + ")";
            ImGui::Text("Video   : %s", vInfo.c_str());
            std::string aInfo = task->hasAudioSource
                ? (task->audioCodec.empty() ? "?" : task->audioCodec) : "-";
            if (task->hasAudioSource && task->audioBitrate > 0)
                aInfo += "  (" + formatBitrate(task->audioBitrate) + ")";
            ImGui::Text("Audio   : %s", aInfo.c_str());
            if (task->duration > 0.0)
                ImGui::Text("Duration: %s", dur);
            else
                ImGui::Text("Duration: -");
        }
        else
        {
            ImGui::TextDisabled("(no source detected)");
        }
    }
}

void MainWindow::renderVideoSection()
{
    Task* task = m_taskManager.current();
    if (!task) return;

    if (ImGui::CollapsingHeader("Video", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!task->hasVideoSource)
        {
            ImGui::BeginDisabled();
            bool dis = false;
            ImGui::Checkbox("Enable Video Output", &dis);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("(no video stream)");
        }
        else
        {
            ImGui::Checkbox("Enable Video Output", &task->videoEnabled);
        }

        if (task->videoEnabled)
        {
            ImGui::Checkbox("Keep Original Resolution", &task->keepResolution);
            if (task->needsPadding)
                ImGui::Checkbox("add black padding", &task->paddingEnabled);

            static std::map<ImGuiID, double> s_fpsHoverStart;
            auto showFpsTip = [&](const char* text)
            {
                ImGuiID id = ImGui::GetItemID();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    double now = ImGui::GetTime();
                    auto it = s_fpsHoverStart.find(id);
                    if (it == s_fpsHoverStart.end())
                        s_fpsHoverStart[id] = now;
                    else if (now - it->second > 1.0)
                    {
                        ImGui::BeginTooltip();
                        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
                        ImGui::TextUnformatted(text);
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                }
                else
                {
                    s_fpsHoverStart.erase(id);
                }
            };

            static const char* fpsParamItems[] = {"-vf", "-r"};
            static const char* fpsParamTips[] = {
                "-vf fps=\nForce frame rate conversion, changes frame count.",
                "-r\nChanges PTS timestamps, frame count unchanged, no new frames."
            };
            ImGui::SetNextItemWidth(60);
            if (ImGui::BeginCombo("##fpsParam", fpsParamItems[task->frameRateParam]))
            {
                for (int i = 0; i < 2; i++)
                {
                    bool selected = (task->frameRateParam == i);
                    if (ImGui::Selectable(fpsParamItems[i], selected))
                        task->frameRateParam = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    showFpsTip(fpsParamTips[i]);
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();

            static const char* fpsItems[] = {"Source", "60", "59.94 (60000/1001)", "30", "29.97 (30000/1001)", "Custom"};
            static const char* fpsTips[] = {
                "Keep original frame rate, no conversion.",
                "Output 60 fps.",
                "Output 59.94 fps (60000/1001).",
                "Output 30 fps.",
                "Output 29.97 fps (30000/1001).",
                "Custom frame rate (1-240)."
            };
            float maxFpsW = 0.0f;
            for (int i = 0; i < 6; i++)
            {
                float w = ImGui::CalcTextSize(fpsItems[i]).x;
                if (w > maxFpsW) maxFpsW = w;
            }
            ImGui::SetNextItemWidth(maxFpsW + 30.0f);
            if (ImGui::BeginCombo("##fps", fpsItems[task->frameRatePreset]))
            {
                for (int i = 0; i < 6; i++)
                {
                    bool selected = (task->frameRatePreset == i);
                    if (ImGui::Selectable(fpsItems[i], selected))
                        task->frameRatePreset = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    showFpsTip(fpsTips[i]);
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (task->frameRatePreset == 5)
            {
                ImGui::SetNextItemWidth(70);
                ImGui::InputDouble("##customFps", &task->customFrameRate, 0.0, 0.0, "%.3f");
                if (task->customFrameRate < 1.0) task->customFrameRate = 1.0;
                if (task->customFrameRate > 240.0) task->customFrameRate = 240.0;
                ImGui::SameLine();
                ImGui::Text("Frame Rate");
            }
            else
            {
                ImGui::Text("Frame Rate");
            }

            ImGui::SetNextItemWidth(70);
            ImGui::InputInt("##crf", &task->crf, 0, 0);
            if (task->crf < 0) task->crf = 0;
            if (task->crf > 63) task->crf = 63;
            ImGui::SameLine();
            ImGui::Text("CRF");
            ImGui::SameLine();
            ImGui::TextDisabled("(0-63)");
            if (!task->keepResolution)
            {
                ImGui::SetNextItemWidth(70);
                ImGui::InputInt("##width", &task->width, 0, 0);
                if (task->width < 1) task->width = 1;
                ImGui::SameLine();
                ImGui::Text("Width");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(70);
                ImGui::InputInt("##height", &task->height, 0, 0);
                if (task->height < 1) task->height = 1;
                ImGui::SameLine();
                ImGui::Text("Height");
            }

            ImGui::SetNextItemWidth(70);
            ImGui::InputFloat("##brightness", &task->brightness, 0.0f, 0.0f, "%.0f");
            if (task->brightness < 0.0f) task->brightness = 0.0f;
            if (task->brightness > 200.0f) task->brightness = 200.0f;
            ImGui::SameLine();
            ImGui::Text("Brightness");
            ImGui::SameLine();
            ImGui::TextDisabled("(0-200)");

            ImGui::SetNextItemWidth(70);
            ImGui::InputInt("##speed", &task->speed, 0, 0);
            if (task->speed < 0) task->speed = 0;
            if (task->speed > 8) task->speed = 8;
            ImGui::SameLine();
            ImGui::Text("Speed");
            ImGui::SameLine();
            ImGui::TextDisabled("(0-8)");

            ImGui::SetNextItemWidth(70);
            ImGui::InputInt("##tilecols", &task->tileColumns, 0, 0);
            if (task->tileColumns < 0) task->tileColumns = 0;
            if (task->tileColumns > 6) task->tileColumns = 6;
            ImGui::SameLine();
            ImGui::Text("Tile Columns");
            ImGui::SameLine();
            ImGui::TextDisabled("(0-6)");
        }
    }
}

void MainWindow::renderAudioSection()
{
    Task* task = m_taskManager.current();
    if (!task) return;

    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!task->hasAudioSource)
        {
            ImGui::BeginDisabled();
            bool dis = false;
            ImGui::Checkbox("Enable Audio Output", &dis);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("(no audio stream)");
        }
        else
        {
            ImGui::Checkbox("Enable Audio Output", &task->audioEnabled);
        }

        if (task->audioEnabled)
        {
            ImGui::SetNextItemWidth(70);
            ImGui::InputFloat("##volume", &task->volume, 0.0f, 0.0f, "%.0f");
            if (task->volume < 0.0f) task->volume = 0.0f;
            if (task->volume > 200.0f) task->volume = 200.0f;
            ImGui::SameLine();
            ImGui::Text("Volume %%");
            ImGui::SameLine();
            ImGui::TextDisabled("(0-200)");

            ImGui::SetNextItemWidth(100);
            static const char* items = "44100 Hz\0"
                                       "48000 Hz\0";
            static const int values[] = { 44100, 48000 };
            int idx = (task->sampleRate == 48000) ? 1 : 0;
            ImGui::Combo("##samplerate", &idx, items);
            task->sampleRate = values[idx];
            ImGui::SameLine();
            ImGui::Text("Sample Rate");
        }

        ImGui::Separator();
        if (ImGui::Button("Copy Original Audio Stream", ImVec2(0, 0)))
        {
            Task* t = m_taskManager.current();
            if (t) startAudioCopy(t);
        }
        ImGui::SameLine();
        if (m_audioCopyRunning)
            ImGui::TextColored(ImVec4(0.50f, 0.72f, 0.93f, 1.0f), "Copying...");
        else if (!m_audioCopyStatus.empty())
            ImGui::TextWrapped("%s", m_audioCopyStatus.c_str());
    }
}

void MainWindow::renderQualitySection()
{
    Task* task = m_taskManager.current();
    if (!task) return;

    if (ImGui::CollapsingHeader("Quality Score", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (task->qualityPhase == Task::QDone)
        {
            ImGui::Text("VMAF:  %.4f", task->vmafScore);
            ImGui::Text("SSIM:  %.6f", task->ssimScore);
            ImGui::Text("PSNR:  %.4f", task->psnrScore);
        }
        else if (task->qualityPhase >= Task::QVmaf && task->qualityPhase <= Task::QPsnr)
        {
            const char* phaseName = (task->qualityPhase == Task::QVmaf) ? "VMAF" :
                                    (task->qualityPhase == Task::QSsim) ? "SSIM" : "PSNR";
            ImGui::Text("Running: %s", phaseName);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.50f, 0.72f, 0.93f, 1.0f));
            char overlay[32];
            snprintf(overlay, sizeof(overlay), "%.0f%%", task->qualityProgress * 100.0f);
            ImGui::ProgressBar(task->qualityProgress, ImVec2(-1, 0), overlay);
            ImGui::PopStyleColor();
            if (task->qualitySpeed > 0.0)
                ImGui::Text("Speed: %.2fx", task->qualitySpeed);
        }
        else if (task->videoEncoded && task->hasVideoSource)
        {
            if (!FFmpegBuilder::outputSizeMatchesSource(*task))
            {
                ImGui::BeginDisabled();
                ImGui::Button("Start", ImVec2(100, 0));
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.20f, 1.0f), "Res Mismatch: Unusable");
            }
            else if (ImGui::Button("Start", ImVec2(100, 0)))
            {
                startQualityCheck(task, m_taskManager.currentIndex());
            }
        }
        else if (!task->videoEncoded)
        {
            ImGui::TextDisabled("(available after encoding)");
        }

        ImGui::Separator();
        if (ImGui::TreeNode("VMAF"))
        {
            ImGui::TextWrapped("Closest to human visual perception");
            ImGui::TextDisabled("95-100: Nearly Lossless");
            ImGui::TextDisabled("90-95:  Very Good");
            ImGui::TextDisabled("80-90:  Good");
            ImGui::TextDisabled("<80:    Noticeable Loss");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("SSIM"))
        {
            ImGui::TextWrapped("Measures image structure similarity");
            ImGui::TextDisabled("0.99-1.00: Nearly Lossless");
            ImGui::TextDisabled("0.98-0.99: Very Good");
            ImGui::TextDisabled("0.95-0.98: Good");
            ImGui::TextDisabled("<0.95:     Noticeable Loss");
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("PSNR"))
        {
            ImGui::TextWrapped("Measures pixel error");
            ImGui::TextDisabled(">45:   Excellent");
            ImGui::TextDisabled("40-45: Very Good");
            ImGui::TextDisabled("35-40: Good");
            ImGui::TextDisabled("<35:   Noticeable Loss");
            ImGui::TreePop();
        }
    }
}

void MainWindow::updateRunningProcesses()
{
    for (auto it = m_frameCounters.begin(); it != m_frameCounters.end(); )
    {
        it->second->readStderr();
        if (!it->second->isRunning())
        {
            Task* task = m_taskManager.get(it->first);
            if (task && it->second->getExitCode() == 0)
                task->totalFrames = std::atoll(it->second->getFullStderr().c_str());
            it = m_frameCounters.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = m_runners.begin(); it != m_runners.end(); )
    {
        it->second->readStderr();
        Task* task = m_taskManager.get(it->first);

        if (!it->second->isRunning())
        {
            int exitCode = it->second->getExitCode();
            bool startedNext = false;

            if (task)
            {
                if (exitCode == 0)
                {
                    if (task->duration <= 0.0)
                        task->duration = ProgressParser::parseDurationLine(it->second->getFullStderr());

                    if (task->phase == Task::VideoPhase)
                        task->videoEncoded = true;

                    if (task->phase == Task::VideoPhase && task->audioEnabled && task->hasAudioSource)
                    {
                        auto audioArgs = FFmpegBuilder::buildAudioArgs(*task);
                        if (!audioArgs.empty())
                        {
                            auto runner = std::make_unique<ProcessRunner>();
                            if (runner->start(audioArgs))
                            {
                                task->phase = Task::AudioPhase;
                                task->progress = 0.0f;
                                logCommand("Audio", *task, audioArgs);
                                it->second = std::move(runner);
                                ++it;
                                startedNext = true;
                            }
                        }
                    }

                    if (!startedNext)
                    {
                        if (task->phase == Task::VideoPhase || task->phase == Task::AudioPhase)
                        {
                            if (task->hasVideoSource && task->videoEnabled && m_wannacriAvailable)
                            {
                                auto usmArgs = FFmpegBuilder::buildUsmArgs(*task);
                                if (!usmArgs.empty())
                                {
                                    auto runner = std::make_unique<ProcessRunner>();
                                    if (runner->start(usmArgs))
                                    {
                                        task->phase = Task::UsmPhase;
                                        task->progress = 0.0f;
                                        logCommand("USM", *task, usmArgs);
                                        it->second = std::move(runner);
                                        ++it;
                                        startedNext = true;
                                    }
                                }
                            }
                        }

                        if (!startedNext)
                        {
                            task->status = Task::Finished;
                            task->progress = 1.0f;

                            if (m_sequentialQueue)
                            {
                                int nextIdx = -1;
                                for (int i = 0; i < m_taskManager.count(); ++i)
                                {
                                    Task* t = m_taskManager.get(i);
                                    if (t && t->status != Task::Finished && t->status != Task::Running
                                        && m_runners.find(i) == m_runners.end()
                                        && m_qualityRunners.find(i) == m_qualityRunners.end())
                                    {
                                        nextIdx = i;
                                        break;
                                    }
                                }
                                if (nextIdx >= 0)
                                    startEncoding(m_taskManager.get(nextIdx), nextIdx);
                                else
                                    m_sequentialQueue = false;
                            }
                        }
                    }
                }
                else
                {
                    task->status = Task::Failed;
                    task->errorMessage = it->second->getFullStderr();
                    if (task->errorMessage.empty())
                        task->errorMessage = "ffmpeg exited with code " + std::to_string(exitCode);
                }
            }
            if (!startedNext)
                it = m_runners.erase(it);
        }
        else
        {
            if (task)
            {
                auto now = std::chrono::steady_clock::now();
                auto st = m_videoStartTimes.find(it->first);
                if (st != m_videoStartTimes.end())
                    task->elapsed = std::chrono::duration<double>(now - st->second).count();

                std::string fullStderr = it->second->getFullStderr();
                if (task->duration <= 0.0)
                    task->duration = ProgressParser::parseDurationLine(fullStderr);
                ProgressInfo info;
                ProgressParser::parseOutput(fullStderr, info);
                if (task->duration > 0.0)
                    task->progress = static_cast<float>(info.currentTime / task->duration);
                else if (task->totalFrames > 0 && info.frame > 0)
                    task->progress = static_cast<float>(static_cast<double>(info.frame) / task->totalFrames);
                task->encodeSpeed = info.speed;
            }
            ++it;
        }
    }

    for (auto it = m_qualityRunners.begin(); it != m_qualityRunners.end(); )
    {
        it->second->readStderr();
        Task* task = m_taskManager.get(it->first);

        if (!it->second->isRunning())
        {
            if (task)
            {
                std::string full = it->second->getFullStderr();

                if (task->qualityPhase == Task::QVmaf)
                {
                    size_t pos = full.find("VMAF score: ");
                    if (pos != std::string::npos)
                        task->vmafScore = std::atof(full.c_str() + pos + 12);

                    auto ssimArgs = FFmpegBuilder::buildSsimArgs(*task);
                    if (!ssimArgs.empty())
                    {
                        auto runner = std::make_unique<ProcessRunner>();
                        if (runner->start(ssimArgs))
                        {
                            task->qualityPhase = Task::QSsim;
                            task->qualityProgress = 0.0f;
                            logCommand("SSIM", *task, ssimArgs);
                            it->second = std::move(runner);
                            ++it;
                            continue;
                        }
                    }
                }
                else if (task->qualityPhase == Task::QSsim)
                {
                    size_t pos = full.find("All:");
                    if (pos != std::string::npos)
                        task->ssimScore = std::atof(full.c_str() + pos + 4);

                    auto psnrArgs = FFmpegBuilder::buildPsnrArgs(*task);
                    if (!psnrArgs.empty())
                    {
                        auto runner = std::make_unique<ProcessRunner>();
                        if (runner->start(psnrArgs))
                        {
                            task->qualityPhase = Task::QPsnr;
                            task->qualityProgress = 0.0f;
                            logCommand("PSNR", *task, psnrArgs);
                            it->second = std::move(runner);
                            ++it;
                            continue;
                        }
                    }
                }
                else if (task->qualityPhase == Task::QPsnr)
                {
                    size_t pos = full.find("average:");
                    if (pos != std::string::npos)
                        task->psnrScore = std::atof(full.c_str() + pos + 8);

                    task->qualityPhase = Task::QDone;
                    task->qualityProgress = 1.0f;
                }
            }
            it = m_qualityRunners.erase(it);
        }
        else
        {
            if (task)
            {
                std::string fullStderr = it->second->getFullStderr();
                if (task->duration <= 0.0)
                    task->duration = ProgressParser::parseDurationLine(fullStderr);
                ProgressInfo info;
                ProgressParser::parseOutput(fullStderr, info);
                if (task->duration > 0.0)
                    task->qualityProgress = static_cast<float>(info.currentTime / task->duration);
                else if (task->totalFrames > 0 && info.frame > 0)
                    task->qualityProgress = static_cast<float>(static_cast<double>(info.frame) / task->totalFrames);
                task->qualitySpeed = info.speed;
            }
            ++it;
        }
    }
    if (m_audioCopyRunner)
    {
        m_audioCopyRunner->readStderr();
        if (!m_audioCopyRunner->isRunning())
        {
            int code = m_audioCopyRunner->getExitCode();
            if (code == 0)
            {
                m_audioCopyStatus = "OK: " + m_audioCopyOutput;
            }
            else
            {
                std::string err = m_audioCopyRunner->getFullStderr();
                if (err.size() > 160)
                    err.resize(160);
                m_audioCopyStatus = "Failed (code " + std::to_string(code) + "): " + err;
            }
            m_audioCopyRunner.reset();
            m_audioCopyRunning = false;
        }
    }
}

void MainWindow::startEncoding(Task* task, int index)
{
    task->errorMessage.clear();
    task->videoEncoded = false;
    task->duration = ProgressParser::getDuration(task->inputPath);
    task->totalFrames = 0;
    if (task->duration <= 0.0 && task->hasVideoSource)
    {
        std::vector<std::string> cfArgs = {
            "ffprobe", "-v", "error", "-select_streams", "v:0",
            "-count_frames", "-show_entries", "stream=nb_read_frames",
            "-of", "default=noprint_wrappers=1:nokey=1", task->inputPath
        };
        auto cfRunner = std::make_unique<ProcessRunner>();
        if (cfRunner->start(cfArgs))
            m_frameCounters[index] = std::move(cfRunner);
    }

    std::vector<std::string> args;
    std::string phaseLabel;
    if (task->videoEnabled && task->hasVideoSource)
    {
        args = FFmpegBuilder::buildVideoArgs(*task);
        task->phase = Task::VideoPhase;
        phaseLabel = "Video";
    }
    else if (task->audioEnabled && task->hasAudioSource)
    {
        args = FFmpegBuilder::buildAudioArgs(*task);
        task->phase = Task::AudioPhase;
        phaseLabel = "Audio";
    }

    if (args.empty()) return;

    auto runner = std::make_unique<ProcessRunner>();
    if (runner->start(args))
    {
        logCommand(phaseLabel, *task, args);
        task->status = Task::Running;
        task->progress = 0.0f;
        task->elapsed = 0.0;
        task->encodeSpeed = 0.0;
        task->vmafScore = 0.0;
        task->ssimScore = 0.0;
        task->psnrScore = 0.0;
        task->qualityPhase = Task::QNone;
        task->qualityProgress = 0.0f;
        task->qualitySpeed = 0.0;
        m_runners[index] = std::move(runner);
        m_videoStartTimes[index] = std::chrono::steady_clock::now();
    }
}

void MainWindow::startQualityCheck(Task* task, int index)
{
    if (!task->hasVideoSource || !task->videoEncoded || !FFmpegBuilder::outputSizeMatchesSource(*task))
        return;

    auto vmafArgs = FFmpegBuilder::buildVmafArgs(*task);
    if (vmafArgs.empty()) return;

    auto runner = std::make_unique<ProcessRunner>();
    if (runner->start(vmafArgs))
    {
        logCommand("VMAF", *task, vmafArgs);
        task->qualityPhase = Task::QVmaf;
        task->qualityProgress = 0.0f;
        task->qualitySpeed = 0.0;
        task->vmafScore = 0.0;
        task->ssimScore = 0.0;
        task->psnrScore = 0.0;
        m_qualityRunners[index] = std::move(runner);
    }
}

void MainWindow::startAudioCopy(Task* task)
{
    if (!task || m_audioCopyRunning)
        return;

    auto args = FFmpegBuilder::buildCopyAudioArgs(*task);
    if (args.empty())
    {
        m_audioCopyStatus = "Failed: no audio stream / invalid input";
        return;
    }

    auto runner = std::make_unique<ProcessRunner>();
    if (runner->start(args))
    {
        m_audioCopyOutput = FFmpegBuilder::copyAudioOutputPath(*task);
        logCommand("AudioCopy", *task, args);
        m_audioCopyRunner = std::move(runner);
        m_audioCopyRunning = true;
        m_audioCopyStatus = "Copying...";
    }
    else
    {
        m_audioCopyStatus = "Failed to start ffmpeg";
    }
}

void MainWindow::renderActionsSection()
{
}

void MainWindow::renderRightPanel()
{
    ImGui::Text("Task Queue");
    ImGui::Separator();

    const float cmdLogH = 160.0f;
    float bottomReserve = (cmdLogH + 30) + 32 + (m_taskManager.count() > 0 ? 34 : 0);
    ImGui::BeginChild("TaskList", ImVec2(0, -bottomReserve), false, ImGuiWindowFlags_NoScrollbar);

    if (m_taskManager.count() == 0)
        ImGui::TextUnformatted("No tasks. Click '+ Add Task' or drag files here.");

    int deleteIndex = -1;

    for (int i = 0; i < m_taskManager.count(); ++i)
    {
        const Task* task = m_taskManager.get(i);

        ImVec2 rowStart = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(0, 2));

        float taskW = ImGui::GetContentRegionAvail().x;

        ImGui::PushID(i);
        if (ImGui::Selectable(task->outputName.c_str(), m_taskManager.currentIndex() == i,
                              ImGuiSelectableFlags_None, ImVec2(taskW - 28, 0)))
        {
            syncToTask();
            m_taskManager.select(i);
            syncFromTask();
        }
        ImGui::SameLine(0, 4);
        if (ImGui::SmallButton("X"))
            deleteIndex = i;
        ImGui::PopID();

        char timeBuf[32];
        formatTime(task->elapsed, timeBuf, sizeof(timeBuf));
        const char* label = statusLabel(task->status);

        float prog = (task->status == Task::Running) ? task->progress
                  : (task->status == Task::Finished) ? 1.0f : 0.0f;

        char overlay[64];
        const char* phaseLabel = "";
        if (task->status == Task::Running)
        {
            snprintf(overlay, sizeof(overlay), "%.0f%%", task->progress * 100.0f);
            if (task->phase == Task::VideoPhase) phaseLabel = "Video";
            else if (task->phase == Task::AudioPhase) phaseLabel = "Audio";
            else if (task->phase == Task::UsmPhase) phaseLabel = "USM";
        }
        else
        {
            snprintf(overlay, sizeof(overlay), "%s", label);
        }

        {
            ImVec4 barColor(0.3f, 0.3f, 0.3f, 1.0f);
            if (task->status == Task::Running)
            {
                if (task->phase == Task::VideoPhase)
                    barColor = ImVec4(0.50f, 0.72f, 0.93f, 1.0f);
                else if (task->phase == Task::AudioPhase)
                    barColor = ImVec4(1.0f, 0.80f, 0.20f, 1.0f);
                else if (task->phase == Task::UsmPhase)
                    barColor = ImVec4(0.60f, 0.20f, 0.80f, 1.0f);
            }
            else if (task->status == Task::Finished)
            {
                barColor = ImVec4(0.30f, 0.69f, 0.31f, 1.0f);
            }
            else if (task->status == Task::Failed)
            {
                barColor = ImVec4(0.96f, 0.21f, 0.31f, 1.0f);
            }
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(prog, ImVec2(-1, 0), overlay);
            ImGui::PopStyleColor();
        }

        {
            ImVec4 color(1, 1, 1, 1);
            if (task->status == Task::Running)      color = ImVec4(0.50f, 0.72f, 0.93f, 1.0f);
            else if (task->status == Task::Finished) color = ImVec4(0.30f, 0.69f, 0.31f, 1.0f);
            else if (task->status == Task::Failed)   color = ImVec4(0.96f, 0.21f, 0.31f, 1.0f);
            else if (task->status == Task::Canceled) color = ImVec4(1.0f, 0.60f, 0.10f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            if (task->status == Task::Running && task->encodeSpeed > 0.0)
                ImGui::Text("%s  %s (%s)  %.2fx", timeBuf, label, phaseLabel, task->encodeSpeed);
            else
                ImGui::Text("%s  %s", timeBuf, label);
            ImGui::PopStyleColor();
        }

        if (task->status == Task::Failed && !task->errorMessage.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.21f, 0.31f, 1.0f));
            ImGui::TextWrapped("%s", task->errorMessage.c_str());
            ImGui::PopStyleColor();
        }

        ImVec2 rowEnd = ImGui::GetCursorScreenPos();
        rowEnd.y += 2;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bgColor = (task->status == Task::Waiting || task->status == Task::Canceled)
            ? IM_COL32(38, 38, 38, 255) : IM_COL32(50, 50, 50, 255);
        dl->AddRectFilled(rowStart, rowEnd, bgColor, 4.0f);
        dl->AddRect(rowStart, rowEnd, IM_COL32(25, 25, 25, 255), 4.0f);
        ImGui::Dummy(ImVec2(0, 3));

        if (i != m_taskManager.count() - 1)
            ImGui::Separator();
    }

    if (ImGui::BeginPopupContextWindow("QueueContext"))
    {
        if (ImGui::MenuItem("Clear Completed"))
        {
            std::map<Task*, std::unique_ptr<ProcessRunner>> savedRunners;
            std::map<Task*, std::unique_ptr<ProcessRunner>> savedQuality;
            std::map<Task*, std::chrono::steady_clock::time_point> savedTimes;

            for (auto& p : m_runners)
            {
                Task* t = m_taskManager.get(p.first);
                if (t) savedRunners[t] = std::move(p.second);
            }
            m_runners.clear();
            for (auto& p : m_qualityRunners)
            {
                Task* t = m_taskManager.get(p.first);
                if (t) savedQuality[t] = std::move(p.second);
            }
            m_qualityRunners.clear();
            for (auto& p : m_videoStartTimes)
            {
                Task* t = m_taskManager.get(p.first);
                if (t) savedTimes[t] = p.second;
            }
            m_videoStartTimes.clear();

            for (int i = m_taskManager.count() - 1; i >= 0; --i)
            {
                Task* t = m_taskManager.get(i);
                if (t && t->status == Task::Finished)
                    m_taskManager.remove(i);
            }

            for (int i = 0; i < m_taskManager.count(); ++i)
            {
                Task* t = m_taskManager.get(i);
                auto rit = savedRunners.find(t);
                if (rit != savedRunners.end())
                    m_runners[i] = std::move(rit->second);
                auto qit = savedQuality.find(t);
                if (qit != savedQuality.end())
                    m_qualityRunners[i] = std::move(qit->second);
                auto tit = savedTimes.find(t);
                if (tit != savedTimes.end())
                    m_videoStartTimes[i] = tit->second;
            }
            syncFromTask();
        }
        if (ImGui::MenuItem("Clear All"))
        {
            for (auto& p : m_runners) p.second->stop();
            for (auto& p : m_qualityRunners) p.second->stop();
            m_runners.clear();
            m_qualityRunners.clear();
            m_videoStartTimes.clear();
            while (m_taskManager.count() > 0)
                m_taskManager.remove(0);
            m_sequentialQueue = false;
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();

    if (deleteIndex >= 0)
    {
        std::map<Task*, std::unique_ptr<ProcessRunner>> savedRunners;
        std::map<Task*, std::unique_ptr<ProcessRunner>> savedQuality;
        std::map<Task*, std::chrono::steady_clock::time_point> savedTimes;

        for (auto& p : m_runners)
        {
            Task* t = m_taskManager.get(p.first);
            if (t) savedRunners[t] = std::move(p.second);
        }
        m_runners.clear();
        for (auto& p : m_qualityRunners)
        {
            Task* t = m_taskManager.get(p.first);
            if (t) savedQuality[t] = std::move(p.second);
        }
        m_qualityRunners.clear();
        for (auto& p : m_videoStartTimes)
        {
            Task* t = m_taskManager.get(p.first);
            if (t) savedTimes[t] = p.second;
        }
        m_videoStartTimes.clear();

        syncToTask();
        m_taskManager.remove(deleteIndex);

        for (int i = 0; i < m_taskManager.count(); ++i)
        {
            Task* t = m_taskManager.get(i);
            auto rit = savedRunners.find(t);
            if (rit != savedRunners.end())
                m_runners[i] = std::move(rit->second);
            auto qit = savedQuality.find(t);
            if (qit != savedQuality.end())
                m_qualityRunners[i] = std::move(qit->second);
            auto tit = savedTimes.find(t);
            if (tit != savedTimes.end())
                m_videoStartTimes[i] = tit->second;
        }
        syncFromTask();
    }

    ImGui::Separator();
    {
        ImGui::TextUnformatted("Command Log");
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu)", m_commandLog.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear##cmdlog"))
            m_commandLog.clear();
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy All"))
        {
            std::string all;
            for (const auto& e : m_commandLog)
            {
                all += "[" + e.time + "] [" + e.taskName + "] " + e.phase + "\n";
                all += e.command + "\n";
            }
            ImGui::SetClipboardText(all.c_str());
        }

        ImGui::BeginChild("CmdLog", ImVec2(0, cmdLogH), true);
        if (m_commandLog.empty())
        {
            ImGui::TextDisabled("No commands have been run yet.");
        }
        else
        {
            float childWrapW = ImGui::GetContentRegionAvail().x;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + childWrapW);
            for (size_t i = 0; i < m_commandLog.size(); ++i)
            {
                const CommandLogEntry& e = m_commandLog[i];

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.50f, 0.50f, 1.0f));
                ImGui::TextUnformatted(("[" + e.time + "] [" + e.taskName + "] " + e.phase).c_str());
                ImGui::PopStyleColor();

                ImVec4 cmdColor(0.90f, 0.90f, 0.90f, 1.0f);
                if (e.phase == "Video")
                    cmdColor = ImVec4(0.50f, 0.72f, 0.93f, 1.0f);
                else if (e.phase == "Audio")
                    cmdColor = ImVec4(1.0f, 0.80f, 0.20f, 1.0f);
                else if (e.phase == "USM")
                    cmdColor = ImVec4(0.60f, 0.20f, 0.80f, 1.0f);
                else
                    cmdColor = ImVec4(0.30f, 0.69f, 0.31f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, cmdColor);
                ImGui::TextUnformatted(e.command.c_str());
                ImGui::PopStyleColor();

                if (i != m_commandLog.size() - 1)
                    ImGui::Separator();
            }
            ImGui::PopTextWrapPos();

            if (m_commandLog.size() != m_commandLogRendered)
            {
                m_commandLogRendered = m_commandLog.size();
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();

    if (ImGui::Button("+  Add Task", ImVec2(-1, 0)))
    {
        auto files = FileDialog::openFiles();
        for (const auto& path : files)
        {
            if (path.empty()) continue;
            Task* t = m_taskManager.add();
            t->inputPath = path;
            t->outputFolder = FileDialog::getParentDir(path);
            t->outputName = FileDialog::buildOutputName(path);
            FFmpegBuilder::detectMediaInfo(*t);
            t->videoEnabled = t->hasVideoSource;
            t->audioEnabled = t->hasAudioSource;
        }
        if (!files.empty())
        {
            m_taskManager.select(m_taskManager.count() - static_cast<int>(files.size()));
            syncFromTask();
        }
    }

    if (m_taskManager.count() > 0)
    {
    ImGui::Separator();

    int curIdx = m_taskManager.currentIndex();
    bool selRunning = (m_runners.find(curIdx) != m_runners.end());

    if (!m_ffmpegAvailable || selRunning)
        ImGui::BeginDisabled();
    if (ImGui::Button("Start", ImVec2(100, 0)))
    {
        Task* task = m_taskManager.current();
        if (task)
        {
            syncToTask();
            std::string err = checkTaskErrors(task);
            if (!err.empty())
            {
                task->errorMessage = err;
                task->status = Task::Failed;
            }
            else
            {
                startEncoding(task, curIdx);
            }
        }
    }
    if (!m_ffmpegAvailable || selRunning)
        ImGui::EndDisabled();

    ImGui::SameLine();

    if (!m_ffmpegAvailable || !selRunning)
        ImGui::BeginDisabled();
    if (ImGui::Button("Cancel", ImVec2(100, 0)))
    {
        auto it = m_runners.find(curIdx);
        if (it != m_runners.end())
        {
            it->second->stop();
            Task* task = m_taskManager.current();
            if (task) task->status = Task::Canceled;
            m_runners.erase(it);
            m_videoStartTimes.erase(curIdx);
        }
        auto qit = m_qualityRunners.find(curIdx);
        if (qit != m_qualityRunners.end())
        {
            qit->second->stop();
            m_qualityRunners.erase(qit);
        }
    }
    if (!m_ffmpegAvailable || !selRunning)
        ImGui::EndDisabled();

    ImGui::SameLine();

    if (m_taskManager.count() >= 2)
    {
        bool anyRunning = false;
        for (int i = 0; i < m_taskManager.count(); ++i)
            if (m_runners.find(i) != m_runners.end() || m_qualityRunners.find(i) != m_qualityRunners.end())
                anyRunning = true;

        if (!m_ffmpegAvailable || anyRunning)
            ImGui::BeginDisabled();
        if (ImGui::Button("Start All", ImVec2(100, 0)))
        {
            m_sequentialQueue = true;
            int firstIdx = -1;
            for (int i = 0; i < m_taskManager.count(); ++i)
            {
                Task* t = m_taskManager.get(i);
                if (t && t->status != Task::Finished && t->status != Task::Running
                    && m_runners.find(i) == m_runners.end()
                    && m_qualityRunners.find(i) == m_qualityRunners.end())
                {
                    firstIdx = i;
                    break;
                }
            }
            if (firstIdx >= 0)
                startEncoding(m_taskManager.get(firstIdx), firstIdx);
            else
                m_sequentialQueue = false;
        }
        if (!m_ffmpegAvailable || anyRunning)
            ImGui::EndDisabled();

        ImGui::SameLine();

        if (!m_ffmpegAvailable || !anyRunning)
            ImGui::BeginDisabled();
        if (ImGui::Button("Cancel All", ImVec2(100, 0)))
        {
            m_sequentialQueue = false;
            for (auto& pair : m_runners)
            {
                pair.second->stop();
                Task* t = m_taskManager.get(pair.first);
                if (t) t->status = Task::Canceled;
            }
            m_runners.clear();
            for (auto& pair : m_qualityRunners)
                pair.second->stop();
            m_qualityRunners.clear();
            m_videoStartTimes.clear();
        }
        if (!m_ffmpegAvailable || !anyRunning)
            ImGui::EndDisabled();
    }
    }
}
