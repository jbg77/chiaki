# chiaki 
This fork of Chiaki combines the works of Alvaromunoz, Jbaiters, Egoistically, and Streetpea to deliver an optimized gaming experience. It includes features such as haptic vibrations and adaptive triggers for a more immersive gaming experience, as well as improved colorimetry. Although haptic vibrations only work with USB and may not function, an emulation function for vibrations has been added for use with both USB and Bluetooth. The software is built using QT6.

## Features:

- Adaptive Triggers (Bluetooth and USB)
- Haptic rumble (only USB, may not work)
- Emulated haptic rumble (works with USB and Bluetooth)
- Built with QT6
- Improved colorimetry.

## Not Working:

- Controller microphone
- Controller speaker.

## Build
The program has been compiled for Mac (Apple Silicon) and can be built from source using CMake, Qt6, QtOpenGL and QtSvg, FFMPEG (libavcodec with H264 is enough), libopus, OpenSSL 1.1, SDL 2, protoc and the protobuf Python library (only used during compilation for Nanopb). To build the program, follow these instructions:

    bash
    Copy code
    git submodule update --init
    mkdir build && cd build
    cmake ..
    make

Enjoy playing games like Astrobots, now fully playable.
