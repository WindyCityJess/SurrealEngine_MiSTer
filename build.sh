#!/bin/sh
# Build Unreal Tournament for the MiSTer in the toolchain container: static SDL2 with the
# hybrid core drivers first (once), then the game.
# usage: ./build.sh [mister|host] [Release|RelWithDebInfo|Debug]
#   mister  ARM binary for the MiSTer     -> build/mister/surrealengine
#   host    same code for this PC, to run against hybrid/tools/fakecore
set -e
cd "$(dirname "$0")"
TARGET=${1:-mister}
TYPE=${2:-Release}
ROOT=$PWD
PREFIX=$ROOT/build/sdl/$TARGET/prefix

hybrid/docker.sh hybrid/sdl2/build.sh "$TARGET" build/sdl

CROSS=
if [ "$TARGET" = mister ]; then
    CROSS="-DCMAKE_TOOLCHAIN_FILE=$ROOT/hybrid/toolchain/mister.cmake -DCMAKE_FIND_ROOT_PATH=$PREFIX"
    # the compiler's runtime is linked in, the binary only needs the MiSTer's glibc
    CROSS="$CROSS -DCMAKE_EXE_LINKER_FLAGS='-no-pie -static-libstdc++ -static-libgcc'"
fi

# The engine's resources (SurrealEngine.pk3, its fonts and icons) are zipped
# by a tool of its own that runs on this PC: for the MiSTer it comes from a
# native build of that target alone, and is handed to the cross build.
PK3=
if [ "$TARGET" = mister ]; then
    hybrid/docker.sh hybrid/sdl2/build.sh host build/sdl
    hybrid/docker.sh sh -c "
      cmake -S surrealengine -B build/pk3 -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=$ROOT/build/sdl/host/prefix -DMISTER_HYBRID=ON -DENABLE_SDL3=OFF &&
      cmake --build build/pk3 --target Resources -j\$(nproc)"
    PK3="-DSURREAL_HOST_PK3=$ROOT/build/pk3/SurrealEngine.pk3"
fi

# MISTER_HYBRID: the software render device, SDL2 alone (no Vulkan, OpenGL,
# X11, OpenAL or ALSA), no editor or debugger
hybrid/docker.sh sh -c "
  cmake -S surrealengine -B build/$TARGET -DCMAKE_BUILD_TYPE=$TYPE $CROSS \
    -DCMAKE_PREFIX_PATH=$PREFIX -DMISTER_HYBRID=ON -DENABLE_SDL3=OFF $PK3 &&
  cmake --build build/$TARGET --target SurrealEngine SoftwareRenderTest -j\$(nproc)"
# the binary under the name the scripts know
cp "build/$TARGET/SurrealEngine" "build/$TARGET/surrealengine"
ls -la "build/$TARGET/surrealengine" "build/$TARGET/SurrealEngine.pk3"

# The launch script's messages on the core's picture (hybrid/tools/mh_message.c)
if [ "$TARGET" = mister ]; then
    hybrid/docker.sh arm-linux-gnueabihf-gcc -O2 -static -Ihybrid/hps -o build/mister/mh_message \
        hybrid/tools/mh_message.c hybrid/hps/*.c -lpthread -lm
fi
