LAC1 AIPU Driver Example Readme
1. Example application source files are in ./src directry;
2. Build the application by executing the following steps:
    >>>cd ./
    >>>make
    >>>you can find the application executable under ./build
3. Execute the application with provided benchmark resnet_50 by:
    >>>./run.sh -c resnet_50
4. A log "[TEST INFO] Test Result Check PASS!" will be printed
    which means that the execution is successful.
5. This example application only support single in/out benchmarks,
    and you may modify it to support multi-in/out cases.
6. Replace the benchmarks, simulator, or libaipu.so in this directory
    if you have other demands.
