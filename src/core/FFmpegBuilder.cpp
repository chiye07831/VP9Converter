#include "FFmpegBuilder.h"
#include "model/Defaults.h"
#include "core/ProcessRunner.h"
#include <cstdio>
#include <cmath>
#include <thread>
#include <cstdlib>
#include <sstream>

namespace FFmpegBuilder {

static bool parseRatio(const std::string& s, double& num, double& den)
{
    std::string::size_type sep = s.find(':');
    if (sep == std::string::npos)
        sep = s.find('/');
    if (sep == std::string::npos)
        return false;
    num = std::atof(s.substr(0, sep).c_str());
    den = std::atof(s.substr(sep + 1).c_str());
    return num > 0.0 && den > 0.0;
}

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

        if (task.needsLetterbox && task.srcWidth > 0 && task.srcHeight > 0)
        {
            int outW, outH;
            if (task.keepResolution)
            {
                outH = task.srcHeight;
                outW = static_cast<int>(std::llround(task.srcHeight * 16.0 / 9.0));
                if (outW % 2 != 0) ++outW;
                if (outW < 2) outW = 2;
            }
            else
            {
                outW = task.width;
                outH = task.height;
            }
            if (outW > 0 && outH > 0)
            {
                char buf[192];
                snprintf(buf, sizeof(buf),
                         "scale=trunc(iw*sar/2)*2:trunc(ih/2)*2,pad=%d:%d:(ow-iw)/2:(oh-ih)/2,setsar=1",
                         outW, outH);
                filters.push_back(buf);
            }
        }
        else if (!task.keepResolution)
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

#ifdef _WIN32
    args.push_back("pythonw");
#else
    args.push_back("python3");
#endif
    args.push_back("-c");
    args.push_back(
        "import sys\n"
        "sys.argv = ['wannacri', 'createusm'] + sys.argv[1:]\n"
        "import subprocess\n"
        "_orig_init = subprocess.Popen.__init__\n"
        "def _patched(self, *a, **kw):\n"
        "    kw['creationflags'] = kw.get('creationflags', 0) | 0x08000000\n"
        "    _orig_init(self, *a, **kw)\n"
        "subprocess.Popen.__init__ = _patched\n"
        "from wannacri.wannacri import create_usm\n"
        "create_usm()\n"
    );
    args.push_back(ivfPath);
    args.push_back("-e");
    args.push_back("utf-8");
    return args;
}

std::string copyAudioOutputPath(const Task& task)
{
    std::string ext = "mka";
    const std::string& c = task.audioCodec;
    if (c == "aac")          ext = "m4a";
    else if (c == "mp3")     ext = "mp3";
    else if (c == "vorbis")  ext = "ogg";
    else if (c == "opus")    ext = "opus";
    else if (c == "flac")    ext = "flac";
    else if (c == "ac3")     ext = "ac3";
    else if (c == "eac3")    ext = "eac3";
    else if (c == "pcm_s16le" || c == "pcm_s24le" || c == "pcm_s32le"
          || c == "pcm_f32le" || c == "pcm_f64le") ext = "wav";
    return makeOutputPath(task, ("_original." + ext).c_str());
}

std::vector<std::string> buildCopyAudioArgs(const Task& task)
{
    std::vector<std::string> args;
    if (task.inputPath.empty() || task.outputFolder.empty() || task.outputName.empty()
        || !task.hasAudioSource)
        return args;

    args.push_back("ffmpeg");
    args.push_back("-y");
    args.push_back("-i");
    args.push_back(task.inputPath);
    args.push_back("-vn");
    args.push_back("-c:a");
    args.push_back("copy");
    args.push_back(copyAudioOutputPath(task));
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

static void parseMediaInfo(Task& task, const std::string& output)
{
    task.hasVideoSource = false;
    task.hasAudioSource = false;
    task.srcWidth = 0;
    task.srcHeight = 0;
    task.dispWidth = 0;
    task.dispHeight = 0;
    task.frameRate = 0.0;
    task.videoCodec.clear();
    task.audioCodec.clear();
    task.needsLetterbox = false;

    bool inStream = false;
    std::string type, codecName, sarStr, rateStr;
    int w = 0, h = 0;

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line == "[STREAM]")
        {
            inStream = true;
            type.clear(); codecName.clear(); sarStr.clear(); rateStr.clear();
            w = h = 0;
            continue;
        }
        if (line == "[/STREAM]")
        {
            inStream = false;
            if (type == "video")
            {
                task.hasVideoSource = true;
                task.videoCodec = codecName;
                task.srcWidth = w;
                task.srcHeight = h;

                double sarNum = 1.0, sarDen = 1.0;
                parseRatio(sarStr, sarNum, sarDen);
                task.dispWidth = static_cast<int>(std::llround(w * sarNum / sarDen));
                task.dispHeight = h;
                if (w > 0 && h > 0)
                {
                    double dar = (static_cast<double>(w) * sarNum) / (static_cast<double>(h) * sarDen);
                    task.needsLetterbox = (std::fabs(dar - 16.0 / 9.0) > 0.02);
                }

                double num = 0.0, den = 0.0;
                if (parseRatio(rateStr, num, den))
                    task.frameRate = num / den;
            }
            else if (type == "audio")
            {
                task.hasAudioSource = true;
                task.audioCodec = codecName;
            }
            continue;
        }
        if (line == "[FORMAT]" || line == "[/FORMAT]")
            continue;

        std::string::size_type eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (inStream)
        {
            if (key == "codec_type")         type = val;
            else if (key == "codec_name")    codecName = val;
            else if (key == "width")         w = std::atoi(val.c_str());
            else if (key == "height")        h = std::atoi(val.c_str());
            else if (key == "sample_aspect_ratio") sarStr = val;
            else if (key == "r_frame_rate")  rateStr = val;
        }
        else
        {
            if (key == "duration")
                task.duration = std::atof(val.c_str());
        }
    }
}

void detectMediaInfo(Task& task)
{
    std::vector<std::string> args;
    args.push_back("ffprobe");
    args.push_back("-v");
    args.push_back("error");
    args.push_back("-show_entries");
    args.push_back("stream=codec_type,codec_name,width,height,sample_aspect_ratio,r_frame_rate:format=duration");
    args.push_back("-of");
    args.push_back("default");
    args.push_back(task.inputPath);

#ifdef _WIN32
    std::string output;
    if (!ProcessRunner::runAndWait(args, output))
        return;
    parseMediaInfo(task, output);
#else
    std::string cmd;
    for (const auto& a : args)
    {
        if (!cmd.empty()) cmd += ' ';
        if (a.find(' ') != std::string::npos)
            cmd += '"' + a + '"';
        else
            cmd += a;
    }
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buf[256] = {};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
    pclose(pipe);
    parseMediaInfo(task, output);
#endif
}

}
