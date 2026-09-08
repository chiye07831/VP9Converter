#pragma once

#include <string>
#include "Defaults.h"

struct Task
{
    std::string inputPath;
    std::string outputFolder;
    std::string outputName;

    bool videoEnabled = true;
    bool hasVideoSource = true;
    bool constantQuality = true;

    int srcWidth = 0;
    int srcHeight = 0;
    int dispWidth = 0;
    int dispHeight = 0;
    double frameRate = 0.0;
    int frameRatePreset = 0;
    int frameRateParam = 0;
    double customFrameRate = 30.0;
    std::string videoCodec;
    std::string audioCodec;
    int64_t videoBitrate = 0;
    int64_t audioBitrate = 0;
    int64_t totalFrames = 0;
    bool needsPadding = false;
    bool paddingEnabled = true;
    bool videoEncoded = false;

    int crf = DEFAULT_CRF;
    int targetBitrate = 0;
    int minBitrate = 0;
    int maxBitrate = 0;
    bool keepResolution = true;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    float brightness = DEFAULT_BRIGHTNESS;
    int speed = DEFAULT_SPEED;
    int tileColumns = DEFAULT_TILE_COLUMNS;

    bool audioEnabled = true;
    bool hasAudioSource = true;
    float volume = DEFAULT_VOLUME;
    int sampleRate = DEFAULT_SAMPLE_RATE;

    enum Status { Waiting, Running, Finished, Canceled, Failed };
    enum Phase { VideoPhase, AudioPhase, UsmPhase };
    enum QualityPhase { QNone, QVmaf, QSsim, QPsnr, QDone };

    Status status = Waiting;
    Phase phase = VideoPhase;
    float progress = 0.0f;
    double elapsed = 0.0;
    double duration = 0.0;
    double encodeSpeed = 0.0;
    std::string errorMessage;

    double vmafScore = 0.0;
    double ssimScore = 0.0;
    double psnrScore = 0.0;
    float qualityProgress = 0.0f;
    double qualitySpeed = 0.0;
    QualityPhase qualityPhase = QNone;
};
