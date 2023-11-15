#!/bin/bash

# Build Chiaki for macOS distribution using dependencies from MacPorts and custom ffmpeg

set -xe
cd $(dirname "${BASH_SOURCE[0]}")/..
scripts/build-ffmpeg.sh
export CMAKE_PREFIX_PATH="`pwd`/ffmpeg-prefix"
scripts/build-common.sh
cp -a build/gui/chiaki.app Chiaki.app
/opt/local/libexec/qt6/bin/macdeployqt Chiaki.app

# Remove all LC_RPATH load commands that have absolute paths of the build machine
RPATHS=$(otool -l Chiaki.app/Contents/MacOS/chiaki | grep -A 2 LC_RPATH | grep 'path /' | awk '{print $2}')
for p in ${RPATHS}; do install_name_tool -delete_rpath "$p" Chiaki.app/Contents/MacOS/chiaki; done

# This may warn because we already ran macdeployqt above
/opt/local/libexec/qt6/bin/macdeployqt Chiaki.app -dmg