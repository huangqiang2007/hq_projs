#!/bin/bash
STANDARD_V_MAJOR=3
STANDARD_V_MINOR=6.5
LL_V_MAJOR=0
LL_V_MINOR=0.2
LIB_V_MAJOR=0
LIB_V_MINOR=0
TEST_V_MAJOR=2
TEST_V_MINOR=0.0
UMD_LIB_DIR=../bin/umd
LIB_LINK_TARGET=libaipudrv.so
DEBUG=release
LIB_TYPE=standard_api
HW_VERSION=z1

set -e

build_help() {
    echo "============================Test Build Help=============================="
    echo "Test Build Options:"
    echo "-h, --help        help"
    echo "-p, --platform    platform supported:"
    echo "                      x86-linux"
    echo "                      juno-linux-4.9"
    echo "                      hybrid-linux-4.9"
    echo "                      440-linux-4.14"
    echo "                      6cg-linux-4.14"
    echo "-t, --toolchain   GCC path"
    echo "-d, --debug       build debug version (release if not specified)"
    echo "-c                build test case:"
    echo "                      simulation_test"
    echo "                      benchmark_test"
    echo "                      multigraph_test"
    echo "                      pipeline_test"
    echo "                      profiling_test"
    echo "                      dump_test"
    echo "                      debugger_test"
    echo "                      multithread_test"
    echo "                      multithread_share_graph_test"
    echo "                      multithread_non_pipeline_test"
    echo "-l, --lib         link lib type:"
    echo "                      standard_api (by default)"
    echo "                      low_level_api"
    echo "-v, --version     target hardware version:"
    echo "                      z1-default"
    echo "                      z2-default"
    echo "========================================================================="
    exit 1
}

if [ $# = 0 ]; then
    build_help
fi

ARGS=`getopt -o hdp:t:c:l:v: --long help,platform:,toolchain:,debug:,lib:,version: -n 'build.sh' -- "$@"`
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
         TDIR="$2"
         shift
         ;;
     -c)
         TEST_CASE="$2"
         MAKE_FLAG="TEST_CASE=$2 $MAKE_FLAG"
         shift
         ;;
     -l|--lib)
         LIB_TYPE="$2"
         MAKE_FLAG="LINK_LIB=$2 $MAKE_FLAG"
         shift
         ;;
     -v|--version)
         HW_VERSION="$2"
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

if [ "$LIB_TYPE"x = "standard_api"x ]; then
    LIB_V_MAJOR=$STANDARD_V_MAJOR
    LIB_V_MINOR=$STANDARD_V_MINOR
elif [ "$LIB_TYPE"x = "low_level_api"x ]; then
    LIB_V_MAJOR=$LL_V_MAJOR
    LIB_V_MINOR=$LL_V_MINOR
else
    echo "[TEST BUILD ERROR] Invalid lib type $LIB_TYPE! Please use "l" to specify a valid lib to be linked!"
    exit 1
fi

LIB_SO_TARGET=$LIB_LINK_TARGET.$LIB_V_MAJOR
LIB_REAL_TARGET=$LIB_SO_TARGET.$LIB_V_MINOR

export PATH=$TDIR:$PATH

if [ "$PLATFORM"x = "juno-linux-4.9"x ]; then
    MAKE_FLAG="TARGET_PLATFORM=$PLATFORM CXX=aarch64-linux-gnu-g++ $MAKE_FLAG"
elif [ "$PLATFORM"x = "hybrid-linux-4.9"x ]; then
    MAKE_FLAG="TARGET_PLATFORM=$PLATFORM CXX=aarch64-linux-c++ $MAKE_FLAG"
elif [ "$PLATFORM"x = "x86-linux"x ]; then
    MAKE_FLAG="TARGET_PLATFORM=$PLATFORM CXX=g++ $MAKE_FLAG"
elif [ "$PLATFORM"x = "440-linux-4.14"x ]; then
    MAKE_FLAG="TARGET_PLATFORM=$PLATFORM CXX=arm-linux-gnueabihf-g++ $MAKE_FLAG"
elif [ "$PLATFORM"x = "6cg-linux-4.14"x ]; then
    MAKE_FLAG="DEBUG_FLAG=$DEBUG CXX=aarch64-linux-gnu-g++ TARGET_PLATFORM=6cg-linux-4.14 $MAKE_FLAG"
else
    echo "[TEST BUILD ERROR] Invalid platform $PLATFORM! Please use "-p" to specify a valid target platform!"
    exit 1
fi

UMD_LIB_DIR=$UMD_LIB_DIR/$LIB_TYPE/$PLATFORM/$DEBUG
BUILD_DIR=./build/$PLATFORM/$TEST_CASE
mkdir -p $BUILD_DIR
rm -f ${BUILD_DIR}/${LIB_LINK_TARGET}*
rm -f ${BUILD_DIR}/aipu_*

if [ -e $UMD_LIB_DIR/$LIB_REAL_TARGET ]; then
    cp $UMD_LIB_DIR/$LIB_REAL_TARGET $BUILD_DIR
else
    echo "[TEST BUILD ERROR] lib $LIB_REAL_TARGET not found in $UMD_LIB_DIR!"
    exit 1
fi

ln -s $LIB_REAL_TARGET $BUILD_DIR/$LIB_SO_TARGET
ln -s $LIB_SO_TARGET   $BUILD_DIR/$LIB_LINK_TARGET

echo "========================Driver Test Building Start======================"
echo "[TEST BUILD INFO] start test case $TEST_CASE building: $DEBUG version [$TEST_V_MAJOR.$TEST_V_MINOR]"
echo "[TEST BUILD INFO] aipu dynamic lib linked: $LIB_TYPE $DEBUG version [$LIB_V_MAJOR.$LIB_V_MINOR]"
echo "[TEST BUILD INFO] target platform is $PLATFORM"
make $MAKE_FLAG

if [ -f $BUILD_DIR/aipu_$TEST_CASE ]; then
    echo "[TEST BUILD INFO] building target done: $BUILD_DIR/aipu_$TEST_CASE"
fi

echo "========================Driver Test Building End========================"
