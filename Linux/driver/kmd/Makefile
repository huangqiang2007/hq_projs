MODULE_NAME := aipu
SRC_DIR := src
EXTRA_CFLAGS += -D$(PLATFORM_FLAG)=1 -D$(VERSION_FLAG)=1 -I$(PWD)/src -I$(PWD)/src/aipu \
                   -I$(PWD)/src/aipu/zhouyi -I$(PWD)/src/aipu/soc -I$(PWD)/src/config

ifeq ($(DEBUG_FLAG), BUILD_DEBUG_VERSION)
	EXTRA_CFLAGS += -DBUILD_DEBUG_VERSION=1
else
    EXTRA_CFLAGS += -DBUILD_DEBUG_VERSION=0
endif

FOPS_OBJ := $(SRC_DIR)/aipu_fops.o \
            $(SRC_DIR)/aipu_session.o
MEM_OBJ  := $(SRC_DIR)/aipu_mm.o
HW_OBJ   := $(SRC_DIR)/aipu/aipu.o \
            $(SRC_DIR)/aipu/aipu_core.o \
            $(SRC_DIR)/aipu/zhouyi/zhouyi.o \
            $(SRC_DIR)/aipu/soc/aipu_soc.o \
            $(SRC_DIR)/aipu_io.o \
            $(SRC_DIR)/aipu_irq.o
JOB_OBJ  := $(SRC_DIR)/aipu_job_manager.o \
            $(SRC_DIR)/aipu_thread_waitqueue.o
MISC_OBJ := $(SRC_DIR)/aipu_errcode_map.o \
            $(SRC_DIR)/aipu_sysfs.o

ifeq ($(VERSION_FLAG), BUILD_ZHOUYI_V1)
    AIPU_OBJ := $(SRC_DIR)/aipu/zhouyi/z1/z1.o
else ifeq ($(VERSION_FLAG), BUILD_ZHOUYI_V2)
    AIPU_OBJ := $(SRC_DIR)/aipu/zhouyi/z2/z2.o
else
    AIPU_OBJ := $(SRC_DIR)/aipu/zhouyi/z1/z1.o \
            $(SRC_DIR)/aipu/zhouyi/z2/z2.o
endif

ifeq ($(PLATFORM_FLAG), BUILD_PLATFORM_JUNO)
    SOC_OBJ := $(SRC_DIR)/aipu/soc/junor2/junor2.o
else ifeq ($(PLATFORM_FLAG), BUILD_PLATFORM_6CG)
    SOC_OBJ := $(SRC_DIR)/aipu/soc/x6cg/x6cg.o
else
    SOC_OBJ := $(SRC_DIR)/aipu/soc/default/default.o
endif

INIT_OBJ := $(SRC_DIR)/aipu_platform_driver.o
OBJS     := $(FOPS_OBJ) \
            $(MEM_OBJ) \
            $(HW_OBJ) \
            $(JOB_OBJ) \
            $(MISC_OBJ) \
            $(AIPU_OBJ) \
            $(SOC_OBJ)

ifneq ($(KERNELRELEASE),)
	obj-m := $(MODULE_NAME).o
	$(MODULE_NAME)-objs := $(INIT_OBJ) $(OBJS)
else
	KDIR ?= /lib/modules/`uname -r`/build
	PWD :=$(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
endif