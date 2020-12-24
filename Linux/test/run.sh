#!/bin/bash
if [ "$(uname -i)" == 'x86_64' ]; then
    TEST=simulation
else
    TEST=benchmark
fi
TEST_BIN_DIR=./${TEST}_test
TEST_APP=aipu_${TEST}_test
BENCHMARK_CASE_DIR=./benchmarks/z2_1104
OUTPUT_DUMP_TOP_DIR=./output
KMD_RELEASE_DIR=.
CHECK=0
TOOLS_DIR=./tools
SIMULATOR_PREFIX=/project/ai/scratch01/AIPU_SIMULATOR/aipu_simulator_
SIMULATOR_HW_VERSION=z1

test_run_help() {
    echo "=====================Driver Test Run Help================================"
    echo "Test Run Options:"
    echo "-h, --help        help"
    echo "-t, --test        test case to run: simulation/benchmark (by default),"
    echo "                    dump, low_level_api"
    echo "-c, --case        benchmark case(s) to run (use default searching path)"
    echo "-v                hardware version (z1/z2, for simulation to use)"
    echo "-C, --check       auto detailed dump file checking"
    echo "========================================================================="
    exit 1
}

if [ $# = 0 ]; then
    test_run_help
fi

ARGS=`getopt -o ht:c:v:C --long help,test:,case:,sim_version:,check -n 'run.sh' -- "$@"`
eval set -- "${ARGS}"

while [ -n "$1" ]
do
    case "$1" in
     -h|--help)
         test_run_help
         ;;
     -t|--test)
         TEST="$2"
         TEST_BIN_DIR=./$2_test
         TEST_APP=aipu_$2_test
         shift
         ;;
     -c|--case)
         case="$2"
         shift
         ;;
     -v|--sim_version)
         SIMULATOR_HW_VERSION="$2"
         shift
         ;;
     -C|--check)
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

if [ ! -d $TEST_BIN_DIR ] && \
   [ ! -e $TEST_BIN_DIR/$TEST_APP ]; then
   echo "[TEST RUN ERROR] Test binary dir $TEST_BIN_DIR or binary $TEST_BIN_DIR/$TEST_APP not exist!"
   exit 1
fi

if [ -d "${BENCHMARK_CASE_DIR}/${case}" ]; then
    OUTPUT_DIR="${OUTPUT_DUMP_TOP_DIR}/${case}"
else
    echo "[TEST RUN ERROR] Case binary dir ${BENCHMARK_CASE_DIR}/${case} not exist!"
    echo "[TEST RUN ERROR] Please use "-c" to specify a valid test case!"
    exit 1
fi

export LD_LIBRARY_PATH=${TEST_BIN_DIR}:$LD_LIBRARY_PATH
mkdir -p ./${OUTPUT_DIR}

if [ "$(uname -i)" == 'x86_64' ]; then
source env_setup.sh
fi

if [ "$(uname -i)" != 'x86_64' ]; then
    if [ ! -e ${KMD_RELEASE_DIR}/aipu.ko ]; then
        echo "[TEST RUN ERROR] .ko not found. Build KMD first before running this test!"
        exit 1
    else
        if [ ! -c /dev/aipu ]; then
            sudo insmod ${KMD_RELEASE_DIR}/aipu.ko
        fi
    fi
fi

ARGS="--bin=${BENCHMARK_CASE_DIR}/${case}/aipu.bin \
    --idata=${BENCHMARK_CASE_DIR}/${case}/input0.bin \
    --check=${BENCHMARK_CASE_DIR}/${case}/output.bin \
    --dump_dir=${OUTPUT_DIR}"
SIM_ARGS="--sim=${SIMULATOR_PREFIX}${SIMULATOR_HW_VERSION} --cfg_dir=./"

echo -e "\033[7m[TEST RUN INFO] Run $TEST test with benchmark ${case}\033[0m"
if [ "$(uname -i)" = 'x86_64' ]; then
    ${TEST_BIN_DIR}/${TEST_APP} $ARGS $SIM_ARGS
else
    ${TEST_BIN_DIR}/${TEST_APP} $ARGS
fi

ret=$?
if [ $ret != 0 ]; then
    SAVED_DIR=${OUTPUT_DIR}-fail-$(date "+%Y-%m-%d-%H-%M-%S")
else
    SAVED_DIR=${OUTPUT_DIR}-pass-$(date "+%Y-%m-%d-%H-%M-%S")
fi
mv $OUTPUT_DIR $SAVED_DIR
echo "[TEST RUN INFO] memory section dump files saved under: $SAVED_DIR"

if [ $CHECK = 1 ]; then
    $TOOLS_DIR/cmp.sh -d $SAVED_DIR
fi

exit $ret
#gdb \
#set args --sim=/project/ai/scratch01/AIPU_SIMULATOR/aipu_simulator_z2 --bin=./benchmarks/1_8_8_320_shape_3/aipu.bin \
#  --idata=./benchmarks/1_8_8_320_shape_3/input0.bin --check=./benchmarks/1_8_8_320_shape_3/output.bin \
#  --dump_dir=./output --cfg_dir=./
#set env LD_LIBRARY_PATH /arm/tools/gnu/gcc/7.3.0/rhe7-x86_64/lib64/:./simulation_test:$LD_LIBRARY_PATH
#dir ./src
