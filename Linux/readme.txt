AIPU Linux Driver Readme
------------------------
AIPU Linux driver consists of two key parts — User Mode Driver (UMD) and Kernel Mode Driver (KMD).

The driver supports scheduling inference tasks on multiple target platforms:
    - x86-Linux (onto AIPU simulator)
    - Arm Linux (Juno-r2 Linux/Cadence hybrid Arm-Linux)

For simulation on x86-Linux, only UMD is needed. For Arm-Linux platforms, both UMD & KMD are needed.

To build driver for target platforms, the toolchains specified in the following section are suggested to be used.

ENVIRONMENTS
------------
* Juno Env. (suggested)
    - Arm Juno-r2-LogicTile development board (https://developer.arm.com/tools-and-software/development-boards/juno-development-board)
* AIPU RTL (must)
    - r1p0 Zhouyiz1 (0904/1002/0701)
* Simulator (must)
    - r1p0 Zhouyiz1 AIPU simulator
* Benchmarks (must)
    - r1p0 Zhouyiz1 benchmarks
* System (suggested)
    - debian version 8.6 Linux kernel 4.9.168 (Juno-r2)
    - Linux kernel 4.9.118 (hybrid)
* Toolchains (suggested)
    - g++ 7.3.0 (x86-Linux)
    - gcc-linaro-6.2.1-2016.11-x86_64_aarch64-linux-gnu (Juno-r2)
    - aarch64-linux-c++ (Buildroot 2017.08) 6.4.0 (hybrid)
* Configurations (suggested)
    - Board: FPGA working frequency <= 30MHz on Juno r2
    - System: please config your Linux kernel with the following configurations:
        -- CONFIG_DMA_CMA=y
        -- CONFIG_CMA_SIZE_MBYTES=<desired dedicated CMA size>
        and modify corresponding CMA configurations in device tree file.

The CMA size to configure depends on your application scenarios. Corresponding reference device tree files are provided for driver porting.

Feature List
------------
* single/multiple graph(s) inference
* multi-process/thread application scheduling supported
* AIPU host pipeline supported

Test Running
------------
<TBD: update before packing!!!>

Recent Release Date and Version
-------------------------------
-April. 20, 2020 (Zhouyiz1 r1p0)
--UMD 3.2.21 (UMD API version 2.0.0)
--KMD 1.1.20

Change/Bug Fix List
-------------------
No.