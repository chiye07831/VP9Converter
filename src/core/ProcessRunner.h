#pragma once

#include <string>
#include <vector>

class ProcessRunner
{
public:
    ProcessRunner();
    ~ProcessRunner();

    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;

    bool start(const std::vector<std::string>& args);
    void stop();
    bool isRunning() const;
    int getExitCode() const;
    std::string readStderr();
    std::string getFullStderr() const;

    static bool runAndWait(const std::vector<std::string>& args, std::string& output);

private:
#ifdef _WIN32
    void* m_processHandle;
    void* m_stderrRead;
    void* m_stderrWrite;
    std::string m_stderrAccum;
#else
    int m_pid;
    int m_exitCode;
    bool m_exited;
    int m_stderrFd;
    std::string m_stderrBuf;
#endif
};
