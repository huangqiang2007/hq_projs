#!/bin/bash
DBG_ARGS=""
DBG_DIR=release

##################################################################
#
# Those paths are for driver developer's specific usage.
#
##################################################################
#JUNO_TDIR=./
#JUNO_KDIR=/home/djs/work/bsp/junor2-kernel/linux-4.9.168
JUNO_KDIR=/project/ai/scratch01/dejsha01/bsp/juno/linux-4.9.168
JUNO_TDIR=/project/ai/scratch01/dejsha01/bsp/juno/gcc/gcc-linaro-6.2.1-2016.11-x86_64_aarch64-linux-gnu/bin/
HYBRID_KDIR=/home/emu/hybrid/hybrid_quickstart_kit/esw/buildroot-2017.08/output/build/linux-4.9.118
HYBRID_TDIR=/home/emu/hybrid/hybrid_quickstart_kit/esw/buildroot-2017.08/output/host/bin/
X440_KDIR=/home/djs/work/toolchain/440/linux-xlnx-440
X440_TDIR=/home/djs/work/toolchain/440/tools/gcc-arm-linux-gnueabi/bin
X6CG_KDIR=/home/djs/work/toolchain/6cg/linux-xlnx-armchina
X6CG_TDIR=/home/djs/work/toolchain/6cg/tools/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin
X86_TDIR=./

set -e

build_help() {
    echo "============================Test Build Help=============================="
    echo "Test Build Options:"
    echo "-h, --help        help"
    echo "-p, --platform    target platform:"
    echo "                      x86-linux"
    echo "                      juno-linux-4.9"
    echo "                      hybrid-linux-4.9"
    echo "                      440-linux-4.14"
    echo "                      6cg-linux-4.14"
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

ARGS=`getopt -o hdp:v: --long help,debug,platform:,version: -n 'build_all.sh' -- "$@"`
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
     -v|--version)
         VERSION_ARGS="-v $2"
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

if [ "$PLATFORM"x = "juno-linux-4.9"x ]; then
    KDIR=$JUNO_KDIR
    TDIR=$JUNO_TDIR
elif [ "$PLATFORM"x = "hybrid-linux-4.9"x ]; then
    KDIR=$HYBRID_KDIR
    TDIR=$HYBRID_TDIR
elif [ "$PLATFORM"x = "440-linux-4.14"x ]; then
    KDIR=${X440_KDIR}
    TDIR=${X440_TDIR}
elif [ "$PLATFORM"x = "6cg-linux-4.14"x ]; then
    KDIR=${X6CG_KDIR}
    TDIR=${X6CG_TDIR}
elif [ "$PLATFORM"x = "x86-linux"x ]; then
    TDIR=${X86_TDIR}
fi

cd ../driver
./build_all.sh -p $PLATFORM $VERSION_ARGS -k $KDIR -t $TDIR $DBG_ARGS
cd -

./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c dump_test
if [ "$PLATFORM"x = "x86-linux"x ]; then
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c simulation_test
else
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c benchmark_test
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c debugger_test
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c pipeline_test
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c multithread_test
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c multithread_share_graph_test
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c multithread_non_pipeline_test
    ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c low_level_api_test -l low_level_api
#   ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c profiling_test
#   ./build.sh -p $PLATFORM $DBG_ARGS -t $TDIR -c multigraph_test
fi

if [ "$PLATFORM"x = "x86-linux"x ]; then
    RELEASE_DIR=./
elif [ "$PLATFORM"x = "juno-linux-4.9"x ]; then
    RELEASE_DIR="/home/djs/work/juno-exec"
elif [ "$PLATFORM"x = "hybrid-linux-4.9"x ]; then
    RELEASE_DIR="./hybrid-exec"
elif [ "$PLATFORM"x = "440-linux-4.14"x ]; then
    RELEASE_DIR="/home/share/440-exec"
elif [ "$PLATFORM"x = "6cg-linux-4.14"x ]; then
    RELEASE_DIR="/home/djs/work/6cg-exec"
fi

mkdir -p $RELEASE_DIR

###copy binaries into release dir

#copy common files for all target platforms
cp -rf ./build/$PLATFORM/dump_test/ $RELEASE_DIR

#copy for specific platform
if [ "$PLATFORM"x = "x86-linux"x ]; then
    #copy test app and .so
    cp -rf ./build/$PLATFORM/simulation_test/ $RELEASE_DIR
else
    #copy test app and .so
    cp -rf ./build/$PLATFORM/benchmark_test/ $RELEASE_DIR
    cp -rf ./build/$PLATFORM/pipeline_test/ $RELEASE_DIR
    cp -rf ./build/$PLATFORM/debugger_test/ $RELEASE_DIR
    cp -rf ./build/$PLATFORM/multithread_test/ $RELEASE_DIR
    cp -rf ./build/$PLATFORM/multithread_share_graph_test/ $RELEASE_DIR
    cp -rf ./build/$PLATFORM/multithread_non_pipeline_test/ $RELEASE_DIR
    cp -rf ./build/$PLATFORM/low_level_api_test/ $RELEASE_DIR
    ##copy .ko
    if [ "$DBG_ARGS"x = "-d"x ]; then
        DBG_DIR=debug
    else
        DBG_DIR=release
    fi
    cp ../bin/kmd/$PLATFORM/$DBG_DIR/aipu.ko $RELEASE_DIR
    #copy test scripts
    cp -f run.sh $RELEASE_DIR
    #others
    if [ "$PLATFORM"x = "juno-linux-4.9"x ]; then
        #copy source file for gdb debug
        cp -rf ../driver/umd/src $RELEASE_DIR
        cp -rf ./src  $RELEASE_DIR
    #elif [ "$PLATFORM"x = "440-linux-4.14"x ]; then
        #nothing to copy
    #elif [ "$PLATFORM"x = "hybrid-linux-4.9"x ]; then
        #nothing to copy
    fi
fi
echo "[TEST BUILD INFO] copy to release dir done: $RELEASE_DIR"
