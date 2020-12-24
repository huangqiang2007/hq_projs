#!/bin/bash

SIMULATOR_LIB_DIR=/project/ai/scratch01/AIPU_SIMULATOR/lib/

if [ "$(uname -i)" == 'x86_64' ]; then
export LD_LIBRARY_PATH=/arm/tools/gnu/gcc/7.3.0/rhe7-x86_64/lib64/:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$SIMULATOR_LIB_DIR:$LD_LIBRARY_PATH
fi