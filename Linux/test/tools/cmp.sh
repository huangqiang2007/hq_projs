#!/bin/bash
OUTPUT_DUMP_TOP_DIR=./output

test_run_help() {
    echo "=====================Driver Test Result Compare Help====================="
    echo "Test Run Options:"
    echo "-h, --help        help"
    echo "-d, --dir         test result dump dir"
    echo "========================================================================="
    exit 1
}

if [ $# = 0 ]; then
    test_run_help
fi

ARGS=`getopt -o hd: --long help,dir: -n 'run.sh' -- "$@"`
eval set -- "${ARGS}"

while [ -n "$1" ]
do
    case "$1" in
     -h|--help)
         test_run_help
         ;;
     -d|--dir)
         OUTPUT_DUMP_TOP_DIR="$2"
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

if [ ! -d $OUTPUT_DUMP_TOP_DIR ]; then
    echo "[CMP ERROR] No such dump file directory exists: $OUTPUT_DUMP_TOP_DIR!"
    exit 0
fi

echo "[CMP INFO] Comparing memory section dumps..."

if [ -f $OUTPUT_DUMP_TOP_DIR/*_Before_Run_Code_Section_*.bin ] && \
   [ -f $OUTPUT_DUMP_TOP_DIR/*_After_Run_Code_Section_*.bin ]; then
   cmp $OUTPUT_DUMP_TOP_DIR/*_Before_Run_Code_Section_*.bin $OUTPUT_DUMP_TOP_DIR/*_After_Run_Code_Section_*.bin
   if [ $? = 0 ]; then
       echo "[CMP INFO] Instruction section dump check PASS!"
   else
       echo "[CMP ERROR] Instruction section dump check FAILED!"
   fi
else
   echo "[CMP ERROR] No instruction section dump files found under: $OUTPUT_DUMP_TOP_DIR!"
fi

if [ -f $OUTPUT_DUMP_TOP_DIR/*_Before_Run_Rodata_Section_*.bin ] && \
   [ -f $OUTPUT_DUMP_TOP_DIR/*_After_Run_Rodata_Section_*.bin ]; then
   cmp $OUTPUT_DUMP_TOP_DIR/*_Before_Run_Rodata_Section_*.bin $OUTPUT_DUMP_TOP_DIR/*_After_Run_Rodata_Section_*.bin
   if [ $? = 0 ]; then
       echo "[CMP INFO] Rodata section dump check PASS!"
   else
       echo "[CMP ERROR] Rodata section dump check FAILED!"
   fi
else
   echo "[CMP ERROR] No rodata section dump files found under: $OUTPUT_DUMP_TOP_DIR!"
fi

id=0
fail=0
while [ -f $OUTPUT_DUMP_TOP_DIR/*_Before_Run_Static_Section"$id"_*.bin ]
do
   cmp $OUTPUT_DUMP_TOP_DIR/*_Before_Run_Static_Section${id}_*.bin $OUTPUT_DUMP_TOP_DIR/*_After_Run_Static_Section${id}_*.bin
   if [ $? != 0 ]; then
       fail=1
       echo "[CMP ERROR] Static section $id dump check FAILED!"
   fi
   id=`expr $id + 1 `
done

if [ $fail = 0 ]; then
    echo "[CMP INFO] Static section dump(s) check PASS! (${id}/${id})"
fi