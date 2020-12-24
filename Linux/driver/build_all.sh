#!/bin/bash
DBG_ARGS=""
BUILD_UMD_FAIL=0
BUILD_KMD_FAIL=0
BUILD_LL_FAIL=0
KDIR=/lib/modules/`uname -r`/build

build_help() {
    echo "============================Driver Build Help============================"
    echo "Driver Build Options:"
    echo "-h, --help        help"
    echo "-p, --platform    platform supported:"
    echo "                      x86-linux"
    echo "                      juno-linux-4.9"
    echo "                      hybrid-linux-4.9"
    echo "                      440-linux-4.14"
    echo "                      6cg-linux-4.14"
    echo "-k, --kdir        Linux kernel source path"
    echo "-t, --toolchain   GCC path"
    echo "-v, --version     AIPU version (build zhouyi compatible if not specified)"
    echo "                      z1"
    echo "                      z2"
    echo "-d, --debug       build debug version (release if not specified)"
    echo "========================================================================="
    exit 1
}

if [ $# = 0 ]; then
    build_help
fi

ARGS=`getopt -o hdp:k:t:v: --long help,platform:,kdir:,toolchain:,version:,debug: -n 'build_all.sh' -- "$@"`
eval set -- "$ARGS"

while [ -n "$1" ]
do
    case "$1" in
     -h|--help)
         build_help
         ;;
     -d|--debug)
         DBG_ARGS="-d"
         ;;
     -p|--platform)
         PLATFORM="$2"
         shift
         ;;
     -k|--kdir)
         KDIR="$2"
         shift
         ;;
     -t|--toolchain)
         TDIR_OPT="-t $2"
         shift
         ;;
     -v|--version)
         VER_OPT="-v $2"
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

if [ $PLATFORM != "x86-linux" ]; then
cd kmd
./build.sh -p $PLATFORM $DBG_ARGS -k $KDIR $TDIR_OPT $VER_OPT
BUILD_KMD_FAIL=$?
cd -
fi

cd umd
./build.sh -p $PLATFORM $DBG_ARGS $TDIR_OPT -l standard_api
BUILD_UMD_FAIL=$?
#if [ $PLATFORM = "x86-linux" ]; then
#./build.sh -p $PLATFORM $DBG_ARGS -l python_api
#fi
if [ $PLATFORM != "x86-linux" ]; then
./build.sh -p $PLATFORM $DBG_ARGS $TDIR_OPT -l low_level_api
BUILD_LL_FAIL=$?
fi
cd -

exit $BUILD_UMD_FAIL || $BUILD_KMD_FAIL || $BUILD_LL_FAIL