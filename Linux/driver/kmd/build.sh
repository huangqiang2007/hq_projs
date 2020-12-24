#!/bin/bash
V_MAJOR=1
V_MINOR=2.35
DEBUG=release
ODIR=../../bin/kmd
KERNEL_DIR=/lib/modules/`uname -r`/build

print_log_start(){
    echo "============================KMD Building Start==========================="
    echo "[KMD BUILD INFO] building $DEBUG version KO [$V_MAJOR.$V_MINOR]..."
    echo "[KMD BUILD INFO] target AIPU version is $AIPU..."
    echo "[KMD BUILD INFO] target platform is $PLATFORM..."
}

print_log_end() {
    echo "============================KMD Building End============================="
}

build_help() {
    echo "=============================KMD Build Help=============================="
    echo "KMD Build Options:"
    echo "-h, --help        help"
    echo "-p, --platform    platform supported:"
    echo "                      juno-linux-4.9"
    echo "                      hybrid-linux-4.9"
    echo "                      440-linux-4.14"
    echo "                      6cg-linux-4.14"
    echo "-k, --kdir        Linux kernel source path"
    echo "-t, --toolchain   GCC path"
    echo "-v, --version     AIPU version (build zhouyi compatible if not specified)"
    echo "                      z1"
    echo "                      z2"
    echo "========================================================================="
    exit 1
}

set -e

if [ $# = 0 ]; then
    build_help
fi

ARGS=`getopt -o hdp:v:k:t: --long help,platform:,version:,kdir:,toolchain: -n 'build.sh' -- "$@"`
eval set -- "${ARGS}"

while [ -n "$1" ]
do
    case "$1" in
     -h|--help)
         build_help
         ;;
     -d)
         DEBUG=debug
         ;;
     -p|--platform)
         PLATFORM="$2"
         shift
         ;;
     -v|--version)
         BUILD="$2"
         shift
         ;;
     -k|--kdir)
         KERNEL_DIR="$2"
         shift
         ;;
     -t|--toolchain)
         TOOL_DIR="$2"
         shift
         ;;
     --)
         shift ;
         break
         ;;
     *)
         break
    esac
    shift
done

export PATH=$TOOL_DIR:$PATH

if [ "$PLATFORM"x == "juno-linux-4.9"x ]; then
    MAKE_FLAGS="ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- PLATFORM_FLAG=BUILD_PLATFORM_JUNO"
elif [ "$PLATFORM"x == "hybrid-linux-4.9"x ]; then
    MAKE_FLAGS="ARCH=arm64 CROSS_COMPILE=aarch64-linux- PLATFORM_FLAG=BUILD_PLATFORM_DEFAULT"
elif [ "$PLATFORM"x == "440-linux-4.14"x ]; then
    MAKE_FLAGS="ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- PLATFORM_FLAG=BUILD_PLATFORM_DEFAULT"
elif [ "$PLATFORM"x == "6cg-linux-4.14"x ]; then
    MAKE_FLAGS="ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- PLATFORM_FLAG=BUILD_PLATFORM_6CG"
else
    echo "[KMD BUILD ERROR] Invalid platform! Please use "-p" to specify a valid target platform!"
    build_help
    exit 1
fi

MAKE_FLAGS+=" KDIR=$KERNEL_DIR"

if [ "$BUILD"x == "z1"x ]; then
    MAKE_FLAGS+=" VERSION_FLAG=BUILD_ZHOUYI_V1"
    AIPU="Zhouyi V1"
elif [ "$BUILD"x == "z2"x ]; then
    MAKE_FLAGS+=" VERSION_FLAG=BUILD_ZHOUYI_V2"
    AIPU="Zhouyi V2"
else
    MAKE_FLAGS+=" VERSION_FLAG=BUILD_ZHOUYI_COMPATIBLE"
    AIPU="Zhouyi V1 & V2 compatible"
fi

if [ "$DEBUG"x == "debug"x ]; then
    MAKE_FLAGS+=" DEBUG_FLAG=BUILD_DEBUG_VERSION"
else
    MAKE_FLAGS+=" DEBUG_FLAG=BUILD_RELEASE_VERSION"
fi

set +e

print_log_start
make $MAKE_FLAGS
if [ $? != 0 ]; then
    echo "[KMD BUILD ERROR] Building FAILED!"
    print_log_end
    exit 4
fi

if [ -f aipu.ko ]; then
    RELEASE_DIR=$ODIR/$PLATFORM/$DEBUG
    rm -rf $RELEASE_DIR/aipu.ko
    mkdir -p $RELEASE_DIR
    cp -f ./aipu.ko $RELEASE_DIR
    echo "[KMD BUILD INFO] built target done: $RELEASE_DIR/aipu.ko"
fi
print_log_end
