# VP9Converter

A lightweight desktop GUI for converting video into Project DIVA-compatible VP9 (.ivf) and OGG audio via FFmpeg,and through WannaCRI create ivf as usm file.

## Requirements

- FFmpeg + FFprobe in system PATH
- WannaCRI in the system Python environment

## Build Requirements

- CMake 3.14+
- C++17 compiler
- OpenGL 3.3+

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Usage

1. Launch VP9Converter
2. Add a video file (+ Add Task)
3. Adjust encoding parameters (CRF, resolution, etc.)
4. Click Start to begin conversion
5. Monitor progress in the task queue