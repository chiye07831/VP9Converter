#include "FFmpegBuilder.h"
#include "model/Defaults.h"
#include <cstdio>
#include <thread>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace FFmpegBuilder {

static std::string makeOutputPath(const Task& task, const char* suffix)
{
    std::string path = task.outputFolder;
    if (!path.empty() && path.back() != '/' && path.back() != '\\')
        path += '/';
    path += task.outputName + std::string(suffix);
    return path;
}

int getCpuThreads()
{
    int n = static_cast<int>(std::thread::hardware_concurrency());
    return (n > 0) ? n : 4;
}

std::vector<std::string> buildVideoArgs(const Task& task)
{
    std::vector<std::string> args;
    if (task.inputPath.empty() || task.outputFolder.empty() || task.outputName.empty())
        return args;

    args.push_back("ffmpeg");
    args.push_back("-y");
    args.push_back("-threads");
    args.push_back(std::to_string(getCpuThreads()));
    args.push_back("-i");
    args.push_back(task.inputPath);

    if (task.videoEnabled)
    {
        args.push_back("-c:v");
        args.push_back("libvpx-vp9");
        args.push_back("-profile:v");
        args.push_back("0");
        args.push_back("-pix_fmt");
        args.push_back("yuv420p");

        args.push_back("-crf");
        args.push_back(std::to_string(task.crf));
        args.push_back("-b:v");
        args.push_back("0");

        args.push_back("-speed");
        args.push_back(std::to_string(task.speed));
        args.push_back("-row-mt");
        args.push_back("1");
        args.push_back("-tile-columns");
        args.push_back(std::to_string(task.tileColumns));

        std::vector<std::string> filters;
        if (task.brightness != DEFAULT_BRIGHTNESS)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "val%+.0f", task.brightness - 100.0f);
            filters.push_back("lutyuv=y=" + std::string(buf));
        }
        if (!task.keepResolution)
        {
            filters.push_back("scale=" + std::to_string(task.width) + ":" + std::to_string(task.height));
        }
        if (!filters.empty())
        {
            std::string vf;
            for (size_t i = 0; i < filters.size(); ++i)
            {
                if (i > 0) vf += ",";
                vf += filters[i];
            }
            args.push_back("-vf");
            args.push_back(vf);
        }
    }
    else
    {
        args.push_back("-vn");
    }

    args.push_back("-an");
    args.push_back(makeOutputPath(task, ".ivf"));
    return args;
}

std::vector<std::string> buildAudioArgs(const Task& task)
{
    std::vector<std::string> args;
    if (task.inputPath.empty() || task.outputFolder.empty() || task.outputName.empty())
        return args;

    args.push_back("ffmpeg");
    args.push_back("-y");
    args.push_back("-threads");
    args.push_back(std::to_string(getCpuThreads()));
    args.push_back("-i");
    args.push_back(task.inputPath);

    args.push_back("-vn");
    args.push_back("-c:a");
    args.push_back("libvorbis");
    args.push_back("-ar");
    args.push_back(std::to_string(task.sampleRate));

    if (task.volume != DEFAULT_VOLUME)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", task.volume / 100.0f);
        args.push_back("-af");
        args.push_back("volume=" + std::string(buf));
    }

    args.push_back(makeOutputPath(task, ".ogg"));
    return args;
}

std::vector<std::string> buildVmafArgs(const Task& task)
{
    std::vector<std::string> args;
    if (task.inputPath.empty() || task.outputFolder.empty() || task.outputName.empty())
        return args;

    std::string ivfPath = makeOutputPath(task, ".ivf");
    std::string threads = std::to_string(getCpuThreads());

    args.push_back("ffmpeg");
    args.push_back("-threads");
    args.push_back(threads);
    args.push_back("-i");
    args.push_back(ivfPath);
    args.push_back("-i");
    args.push_back(task.inputPath);
    args.push_back("-lavfi");
    args.push_back("libvmaf=n_threads=" + threads);
    args.push_back("-f");
    args.push_back("null");
    args.push_back("-");
    return args;
}

std::vector<std::string> buildSsimArgs(const Task& task)
{
    std::vector<std::string> args;
    if (task.inputPath.empty() || task.outputFolder.empty() || task.outputName.empty())
        return args;

    std::string ivfPath = makeOutputPath(task, ".ivf");
    std::string threads = std::to_string(getCpuThreads());

    args.push_back("ffmpeg");
    args.push_back("-threads");
    args.push_back(threads);
    args.push_back("-i");
    args.push_back(ivfPath);
    args.push_back("-i");
    args.push_back(task.inputPath);
    args.push_back("-lavfi");
    args.push_back("ssim");
    args.push_back("-f");
    args.push_back("null");
    args.push_back("-");
    return args;
}

std::vector<std::string> buildPsnrArgs(const Task& task)
{
    std::vector<std::string> args;
    if (task.inputPath.empty() || task.outputFolder.empty() || task.outputName.empty())
        return args;

    std::string ivfPath = makeOutputPath(task, ".ivf");
    std::string threads = std::to_string(getCpuThreads());

    args.push_back("ffmpeg");
    args.push_back("-threads");
    args.push_back(threads);
    args.push_back("-i");
    args.push_back(ivfPath);
    args.push_back("-i");
    args.push_back(task.inputPath);
    args.push_back("-lavfi");
    args.push_back("psnr");
    args.push_back("-f");
    args.push_back("null");
    args.push_back("-");
    return args;
}

std::vector<std::string> buildUsmArgs(const Task& task)
{
    std::vector<std::string> args;
    if (task.outputFolder.empty() || task.outputName.empty())
        return args;

    std::string ivfPath = makeOutputPath(task, ".ivf");
    std::string escaped = ivfPath;
    for (size_t p = escaped.find('\\'); p != std::string::npos; p = escaped.find('\\', p + 2))
        escaped.replace(p, 1, "\\\\");

#ifdef _WIN32
    args.push_back("pythonw");
#else
    args.push_back("python3");
#endif
    args.push_back("-c");
    args.push_back(
        "import sys\n"
        "sys.argv = ['wannacri', 'createusm', r'" + escaped + "']\n"
        "import subprocess\n"
        "_orig_init = subprocess.Popen.__init__\n"
        "def _patched(self, *a, **kw):\n"
        "    kw['creationflags'] = kw.get('creationflags', 0) | 0x08000000\n"
        "    _orig_init(self, *a, **kw)\n"
        "subprocess.Popen.__init__ = _patched\n"
        "from wannacri.wannacri import create_usm\n"
        "create_usm()\n"
    );
    return args;
}

std::string commandString(const std::vector<std::string>& args)
{
    std::string result;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0) result += ' ';
        if (args[i].find(' ') != std::string::npos)
        {
            result += '"';
            result += args[i];
            result += '"';
        }
        else
        {
            result += args[i];
        }
    }
    return result;
}

#ifdef _WIN32

void detectMediaInfo(Task& task)
{
    std::string cmd = "ffprobe -v error -show_entries stream=codec_type -of default=noprint_wrappers=1:nokey=1 \"";
    cmd += task.inputPath + "\"";

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return;
    }

    CloseHandle(hWrite);
    CloseHandle(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    char buf[256] = {};
    DWORD totalRead = 0;
    DWORD bytesRead;
    while (ReadFile(hRead, buf + totalRead, sizeof(buf) - totalRead - 1, &bytesRead, nullptr) && bytesRead > 0)
        totalRead += bytesRead;
    buf[totalRead] = '\0';
    CloseHandle(hRead);

    std::string output(buf);
    task.hasVideoSource = (output.find("video") != std::string::npos);
    task.hasAudioSource = (output.find("audio") != std::string::npos);
}

#else

void detectMediaInfo(Task& task)
{
    std::string cmd = "ffprobe -v error -show_entries stream=codec_type -of default=noprint_wrappers=1:nokey=1 \"";
    cmd += task.inputPath + "\" 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buf[256] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);

    task.hasVideoSource = (output.find("video") != std::string::npos);
    task.hasAudioSource = (output.find("audio") != std::string::npos);
}

#endif

}
