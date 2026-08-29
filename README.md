# VP9Converter

A lightweight desktop GUI for converting video into Project DIVA-compatible VP9 (.ivf) and OGG audio via FFmpeg, and through WannaCRI create ivf as usm file. IVF is used because it is the only container WannaCRI accepts for USM packing. Non-16:9 anamorphic sources are de-anamorphed and padded to 16:9 square pixels by the encoder filter chain, so IVF's inability to store SAR is irrelevant.

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