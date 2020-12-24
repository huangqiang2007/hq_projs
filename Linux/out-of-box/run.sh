#!/bin/bash
TEST_APP=aipu_simulation_test
TEST_APP_DIR=./build
UMD_DIR=./umd
BENCHMARK_CASE_DIR=./benchmarks/z2_1104
OUTPUT_DUMP_TOP_DIR=./output

test_run_help() {
    echo "=====================Driver Test Run Help================================"
    echo "Test Run Options:"
    echo "-h, --help        help"
    echo "-c, --case        benchmark case(s) to run (use default searching path)"
    echo "========================================================================="
    exit 1
}

if [ $# = 0 ]; then
    test_run_help
fi

ARGS=`getopt -o hc: --long help,case: -n 'run.sh' -- "$@"`
eval set -- "${ARGS}"

while [ -n "$1" ]
do
    case "$1" in
     -h|--help)
         test_run_help
         ;;
     -c|--case)
         case="$2"
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

if [ ! -d $TEST_APP_DIR ] && \
   [ ! -e $TEST_APP_DIR/$TEST_APP ]; then
   echo "[TEST RUN ERROR] Test binary dir $TEST_APP_DIR or binary $TEST_APP_DIR/$TEST_APP not exist!"
   exit 1
fi

if [ -d "${BENCHMARK_CASE_DIR}/${case}" ]; then
    OUTPUT_DIR="${OUTPUT_DUMP_TOP_DIR}/${case}"
else
    echo "[TEST RUN ERROR] Case binary dir ${BENCHMARK_CASE_DIR}/${case} not exist!"
    echo "[TEST RUN ERROR] Please use "-c" to specify a valid test case!"
    exit 1
fi

export LD_LIBRARY_PATH=${UMD_DIR}:$LD_LIBRARY_PATH
mkdir -p ./${OUTPUT_DIR}

ARGS="--bin=${BENCHMARK_CASE_DIR}/${case}/aipu.bin \
    --idata=${BENCHMARK_CASE_DIR}/${case}/input0.bin \
    --check=${BENCHMARK_CASE_DIR}/${case}/output.bin \
    --dump_dir=${OUTPUT_DIR}"
SIM_ARGS="--sim=./aipu_simulator --cfg_dir=./"

echo -e "\033[7m[TEST RUN INFO] Run $TEST test with benchmark ${case}\033[0m"
${TEST_APP_DIR}/${TEST_APP} $ARGS $SIM_ARGS

ret=$?
if [ $ret != 0 ]; then
    SAVED_DIR=${OUTPUT_DIR}-fail-$(date "+%Y-%m-%d-%H-%M-%S")
else
    SAVED_DIR=${OUTPUT_DIR}-pass-$(date "+%Y-%m-%d-%H-%M-%S")
fi
mv $OUTPUT_DIR $SAVED_DIR
echo "[TEST RUN INFO] memory section dump files saved under: $SAVED_DIR"

exit $ret
