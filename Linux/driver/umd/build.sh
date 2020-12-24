#!/bin/bash
STANDARD_V_MAJOR=3
STANDARD_V_MINOR=6.5
LL_V_MAJOR=0
LL_V_MINOR=0.2
PY_V_MAJOR=0
PY_V_MINOR=0.3
V_MAJOR=0
V_MINOR=0.0
DEBUG=release
CHECK=0
ODIR=../../bin/umd
LIB_TYPE=standard_api
SO_NAME=libaipudrv.so
PY_TARGET_NAME=_$SO_NAME

set -e

print_log_start(){
    echo "============================UMD Building Start==========================="
    echo "[UMD BUILD INFO] building $DEBUG version $LIB_TYPE [$V_MAJOR.$V_MINOR] ..."
    echo "[UMD BUILD INFO] target platform is $2..."
    if [ $CHECK = 1 ]; then
        echo "[UMD BUILD INFO] code check options enabled."
    fi
}

print_log_end() {
    echo "============================UMD Building End============================="
}

build_help() {
    echo "=============================UMD Build Help=============================="
    echo "Runtime Build Options:"
    echo "-h, --help        help"
    echo "-d, --debug       build debug version (release if not specified)"
    echo "-p, --platform    platform supported:"
    echo "                      x86-linux"
    echo "                      juno-linux-4.9"
    echo "                      hybrid-linux-4.9"
    echo "                      440-linux-4.14"
    echo "                      6cg-linux-4.14"
    echo "-t, --toolchain   GCC path"
    echo "-l, --lib         build lib type:"
    echo "                      standard_api (by default)"
    echo "                      low_level_api"
    echo "                      python_api"
    echo "--cc              enable compiler code check options"
    echo "========================================================================="
    exit 1
}

set -e

if [ $# = 0 ]; then
    build_help
fi

ARGS=`getopt -o hdp:t:l: --long help,debug,platform:,toolchain:,cc,lib: -n 'build.sh' -- "$@"`
eval set -- "$ARGS"

while [ -n "$1" ]
do
    case "$1" in
     -h|--help)
         build_help
         ;;
     -d|--debug)
         DEBUG=debug;
         ;;
     -p|--platform)
         PLATFORM="$2"
         shift
         ;;
     -t|--toolchain)
         TOOL_DIR="$2"
         shift
         ;;
     -l|--lib)
         LIB_TYPE="$2"
         shift
         ;;
     --cc)
         CHECK=1
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

if [ "$LIB_TYPE"x = "standard_api"x ]; then
    V_MAJOR=$STANDARD_V_MAJOR
    V_MINOR=$STANDARD_V_MINOR
elif [ "$LIB_TYPE"x = "low_level_api"x ]; then
    V_MAJOR=$LL_V_MAJOR
    V_MINOR=$LL_V_MINOR
elif [ "$LIB_TYPE"x = "python_api"x ]; then
    V_MAJOR=$PY_V_MAJOR
    V_MINOR=$PY_V_MINOR
else
    echo "[LIB BUILD ERROR] Invalid lib type! Please use "-l" to specify a valid lib type to build!"
    exit 1
fi

TARGET_NAME=$SO_NAME.$V_MAJOR.$V_MINOR

if [ "$LIB_TYPE"x = "low_level_api"x ] &&
   [ "$PLATFORM"x = "x86-linux"x ]; then
    echo "[LIB BUILD ERROR] Low level lib only support Arm-Linux platform!"
    exit 1
fi

if [ "$LIB_TYPE"x = "python_api"x ]; then
    PY_EXE="py"
    TARGET_NAME=$PY_TARGET_NAME
else
    PY_EXE=""
fi

export PATH=$TOOL_DIR:$PATH

MAKE_FLAG="MAJOR=$V_MAJOR MINOR=$V_MINOR CODE_CHECK=$CHECK BUILD_LIB_TYPE=$LIB_TYPE"
if [ "$PLATFORM"x = "x86-linux"x ]; then
    MAKE_FLAG="DEBUG_FLAG=$DEBUG CXX=g++ TARGET_PLATFORM=x86-linux $MAKE_FLAG"
elif [ "$PLATFORM"x = "juno-linux-4.9"x ]; then
    MAKE_FLAG="DEBUG_FLAG=$DEBUG CXX=aarch64-linux-gnu-gcc TARGET_PLATFORM=juno-linux-4.9 $MAKE_FLAG"
elif [ "$PLATFORM"x = "hybrid-linux-4.9"x ]; then
    MAKE_FLAG="DEBUG_FLAG=$DEBUG CXX=aarch64-linux-c++ TARGET_PLATFORM=hybrid-linux-4.9 $MAKE_FLAG"
elif [ "$PLATFORM"x = "440-linux-4.14"x ]; then
    MAKE_FLAG="DEBUG_FLAG=$DEBUG CXX=arm-linux-gnueabihf-g++ TARGET_PLATFORM=440-linux-4.14 $MAKE_FLAG"
elif [ "$PLATFORM"x = "6cg-linux-4.14"x ]; then
    MAKE_FLAG="DEBUG_FLAG=$DEBUG CXX=aarch64-linux-gnu-g++ TARGET_PLATFORM=6cg-linux-4.14 $MAKE_FLAG"
else
    echo "[LIB BUILD ERROR] Invalid platform! Please use "-p" to specify a valid target platform!"
    exit 1
fi

BUILD_DIR=./build/$PLATFORM/$DEBUG
mkdir -p $BUILD_DIR
rm -rf $BUILD_DIR/*

set +e

print_log_start $DEBUG $PLATFORM $LIB_TYPE
make $PY_EXE $MAKE_FLAG
if [ $? != 0 ]; then
    echo "[UMD BUILD ERROR] Building FAILED!"
    print_log_end
    exit 2
fi

if [ -f $BUILD_DIR/$TARGET_NAME ]; then
    TGT_DIR=$ODIR/$LIB_TYPE/$PLATFORM/$DEBUG
    echo "[UMD BUILD INFO] building target done: $BUILD_DIR/$TARGET_NAME"
    rm -rf $TGT_DIR/$TARGET_NAME
    mkdir -p $TGT_DIR
    cp -f $BUILD_DIR/$TARGET_NAME $TGT_DIR
    echo "[UMD BUILD INFO] copy target to release dir. done: $TGT_DIR/$TARGET_NAME"
fi
print_log_end
