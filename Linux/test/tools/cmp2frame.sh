#!/bin/bash

OUTPUT_DUMP_TOP_DIR0=./fail
OUTPUT_DUMP_TOP_DIR1=./pass

test_run_help() {
    echo "=====================Driver Test Result Compare Help====================="
    echo "Test Run Options:"
    echo "-h, --help        help"
    echo "-p, --dir0       test pass result dump dir"
    echo "-f, --dir1       test fail result dump dir"
    echo "========================================================================="
    exit 1
}

if [ $# = 0 ]; then
    test_run_help
fi

ARGS=`getopt -o hd:p:f: --long help,dir0:,dir1: -n 'run.sh' -- "$@"`
eval set -- "${ARGS}"

while [ -n "$1" ]
do
    case "$1" in
     -h|--help)
         test_run_help
         ;;
     -p|--dir0)
         OUTPUT_DUMP_TOP_DIR0="$2"
         shift
         ;;
     -f|--dir1)
         OUTPUT_DUMP_TOP_DIR1="$2"
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

if [ ! -d $OUTPUT_DUMP_TOP_DIR0 ]; then
    echo "[CMP ERROR] No such dump file directory exists: $OUTPUT_DUMP_TOP_DIR0!"
    exit 1
elif [ ! -d $OUTPUT_DUMP_TOP_DIR1 ]; then
    echo "[CMP ERROR] No such dump file directory exists: $OUTPUT_DUMP_TOP_DIR1!"
    exit 1
fi

echo "[CMP INFO] Comparing memory section dumps between 2 dirs..."
#check instruction section dumps
if [ -f $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Code_Section_*.bin ] && \
   [ -f $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Code_Section_*.bin ]; then
   cmp $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Code_Section_*.bin $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Code_Section_*.bin
   if [ $? = 0 ]; then
       echo -e "\033[7m[CMP INFO] Instruction section dump after-run check between 2 dirs PASS!\033[0m"
   else
       echo -e "\033[7m[CMP ERROR] Instruction section dump after-run check between 2 dirs FAILED!\033[0m"
   fi
else
   echo "[CMP INFO] No instruction section dump files found under: $OUTPUT_DUMP_TOP_DIR0 or $OUTPUT_DUMP_TOP_DIR1!"
fi

#check stack section dumps
if [ -f $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Stack_Section_*.bin ] && \
   [ -f $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Stack_Section_*.bin ]; then
   cmp $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Stack_Section_*.bin $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Stack_Section_*.bin
   if [ $? = 0 ]; then
       echo -e "\033[7m[CMP INFO] Stack section dump after-run check between 2 dirs PASS!\033[0m"
   else
       echo -e "\033[7m[CMP ERROR] Stack section dump after-run check between 2 dirs FAILED!\033[0m"
   fi
else
   echo "[CMP INFO] No stack section dump files found under: $OUTPUT_DUMP_TOP_DIR0 or $OUTPUT_DUMP_TOP_DIR1!"
fi

#check rodata section dumps
if [ -f $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Rodata_Section_*.bin ] && \
   [ -f $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Rodata_Section_*.bin ]; then
   cmp $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Rodata_Section_*.bin $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Rodata_Section_*.bin
   if [ $? = 0 ]; then
       echo -e "\033[7m[CMP INFO] Rodata section dump after-run check between 2 dirs check PASS!\033[0m"
   else
       echo -e "\033[7m[CMP ERROR] Rodata section dump after-run check between 2 dirs check FAILED!\033[0m"
   fi
else
   echo "[CMP INFO] No rodata section dump after-run files found under: $OUTPUT_DUMP_TOP_DIR0 or $OUTPUT_DUMP_TOP_DIR1!"
fi

#check static section dumps
id=0
fail=0
while [ -f $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Static_Section"$id"_*.bin ] && \
      [ -f $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Static_Section"$id"_*.bin ]
do
   cmp $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Static_Section${id}_*.bin $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Static_Section${id}_*.bin
   if [ $? != 0 ]; then
       fail=1
       echo -e "\033[7m[CMP ERROR] Static section $id dump after-run check between 2 dirs FAILED!\033[0m"
   fi
   id=`expr $id + 1 `
done

if [ $id = 0 ]; then
    echo "[CMP INFO] No static section after-run dump file(s) found!"
else
    if [ $fail = 0 ]; then
        echo -e "\033[7m[CMP INFO] Static section after-run dump(s) check between 2 dirs PASS! (${id}/${id})\033[0m"
    fi
fi

#check reuse section dumps
id=0
fail=0
while [ -f $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Reuse_Section"$id"_*.bin ] && \
      [ -f $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Reuse_Section"$id"_*.bin ]
do
   cmp $OUTPUT_DUMP_TOP_DIR0/*_After_Run_Reuse_Section${id}_*.bin $OUTPUT_DUMP_TOP_DIR1/*_After_Run_Reuse_Section${id}_*.bin
   if [ $? != 0 ]; then
       fail=1
       echo -e "\033[7m[CMP ERROR] Reuse section $id dump after-run check between 2 dirs FAILED!\033[0m"
   fi
   id=`expr $id + 1 `
done

if [ $id = 0 ]; then
    echo "[CMP INFO] No reuse section after-run dump file(s) found!"
else
    if [ $fail = 0 ]; then
        echo -e "\033[7m[CMP INFO] Reuse section after-run dump(s) check between 2 dirs PASS! (${id}/${id})\033[0m"
    fi
fi