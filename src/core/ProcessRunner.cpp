#include "ProcessRunner.h"
#include "util/WinConv.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#endif

ProcessRunner::ProcessRunner()
#ifdef _WIN32
    : m_processHandle(nullptr)
    , m_stderrRead(nullptr)
    , m_stderrWrite(nullptr)
#else
    : m_pid(-1)
    , m_exitCode(0)
    , m_exited(false)
    , m_stderrFd(-1)
#endif
{
}

ProcessRunner::~ProcessRunner()
{
    if (isRunning())
        stop();
#ifdef _WIN32
    if (m_stderrRead) CloseHandle(m_stderrRead);
    if (m_stderrWrite) CloseHandle(m_stderrWrite);
#else
    if (m_stderrFd >= 0) close(m_stderrFd);
#endif
}

#ifdef _WIN32

bool ProcessRunner::start(const std::vector<std::string>& args)
{
    if (args.empty())
        return false;

    std::wstring cmdLine = WinConv::buildCommandLine(args);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = nullptr;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(nullptr, &cmdLine[0], nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(hWrite);
    m_processHandle = pi.hProcess;
    m_stderrRead = hRead;
    m_stderrWrite = nullptr;
    return true;
}

void ProcessRunner::stop()
{
    if (!m_processHandle)
        return;
    TerminateProcess(m_processHandle, 1);
    WaitForSingleObject(m_processHandle, INFINITE);
    CloseHandle(m_processHandle);
    m_processHandle = nullptr;
    if (m_stderrRead)
    {
        CloseHandle(m_stderrRead);
        m_stderrRead = nullptr;
    }
}

bool ProcessRunner::isRunning() const
{
    if (!m_processHandle)
        return false;
    DWORD code;
    if (!GetExitCodeProcess(m_processHandle, &code))
        return false;
    return code == STILL_ACTIVE;
}

int ProcessRunner::getExitCode() const
{
    if (!m_processHandle)
        return -1;
    DWORD code;
    if (!GetExitCodeProcess(m_processHandle, &code))
        return -1;
    return static_cast<int>(code);
}

std::string ProcessRunner::readStderr()
{
    std::string result;
    if (!m_stderrRead)
        return result;

    DWORD bytesAvail = 0;
    if (!PeekNamedPipe(m_stderrRead, nullptr, 0, nullptr, &bytesAvail, nullptr))
        return result;

    if (bytesAvail == 0)
        return result;

    char buf[4096];
    DWORD bytesRead;
    while (bytesAvail > 0)
    {
        DWORD toRead = (bytesAvail > sizeof(buf)) ? sizeof(buf) : bytesAvail;
        if (ReadFile(m_stderrRead, buf, toRead, &bytesRead, nullptr) && bytesRead > 0)
        {
            result.append(buf, bytesRead);
            bytesAvail -= bytesRead;
        }
        else
            break;
    }
    m_stderrAccum += result;
    return result;
}

std::string ProcessRunner::getFullStderr() const
{
    return m_stderrAccum;
}

bool ProcessRunner::runAndWait(const std::vector<std::string>& args, std::string& output)
{
    ProcessRunner runner;
    if (!runner.start(args))
        return false;

    while (runner.isRunning())
    {
        runner.readStderr();
#ifdef _WIN32
        Sleep(1);
#else
        usleep(1000);
#endif
    }
    runner.readStderr();
    output = runner.getFullStderr();
    return true;
}

#else

bool ProcessRunner::start(const std::vector<std::string>& args)
{
    int pipefd[2];
    if (pipe(pipefd) < 0)
        return false;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        std::vector<char*> cargs;
        for (const auto& a : args)
            cargs.push_back(const_cast<char*>(a.c_str()));
        cargs.push_back(nullptr);

        execvp(cargs[0], cargs.data());
        _exit(1);
    }

    close(pipefd[1]);
    m_pid = pid;
    m_exited = false;
    m_exitCode = 0;
    m_stderrFd = pipefd[0];
    m_stderrBuf.clear();

    int flags = fcntl(m_stderrFd, F_GETFL, 0);
    fcntl(m_stderrFd, F_SETFL, flags | O_NONBLOCK);
    return true;
}

void ProcessRunner::stop()
{
    if (m_pid > 0 && !m_exited)
    {
        kill(m_pid, SIGTERM);
        int status;
        waitpid(m_pid, &status, 0);
        m_exited = true;
        m_exitCode = -1;
    }
    m_pid = -1;
    if (m_stderrFd >= 0)
    {
        close(m_stderrFd);
        m_stderrFd = -1;
    }
}

bool ProcessRunner::isRunning() const
{
    if (m_pid <= 0 || m_exited)
        return false;

    int status;
    pid_t result = waitpid(m_pid, &status, WNOHANG);
    if (result == 0)
        return true;

    if (result == m_pid)
    {
        const_cast<ProcessRunner*>(this)->m_exited = true;
        const_cast<ProcessRunner*>(this)->m_exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return false;
}

int ProcessRunner::getExitCode() const
{
    return m_exitCode;
}

std::string ProcessRunner::readStderr()
{
    if (m_stderrFd < 0)
        return {};

    char buf[4096];
    ssize_t n = read(m_stderrFd, buf, sizeof(buf));
    if (n > 0)
    {
        m_stderrBuf.append(buf, n);
        return std::string(buf, n);
    }
    return {};
}

std::string ProcessRunner::getFullStderr() const
{
    return m_stderrBuf;
}

#endif
