MD  := mkdir -p
RM  := rm -rf

#user should update if necessary
PY_INCD_PATH := /usr/include/python3.5m

SRC_ROOT := ./src
BUILD := ./build
BUILD_DIR := $(BUILD)/$(TARGET_PLATFORM)/$(DEBUG_FLAG)
CXXFLAGS := -std=c++11 -fPIC -Wall -Werror
INCD := -I$(SRC_ROOT)
LDFLAGS = -shared -Wl,-soname,$(TARGET_SO_NAME) -lpthread
SO_NAME := libaipudrv.so

ifeq ($(CODE_CHECK), 1)
CXXFLAGS += -fsanitize=signed-integer-overflow -Wfloat-equal -Wundef -Wshadow -Wsizeof-pointer-memaccess -Wswitch-default \
            -Wpointer-arith -Wcast-qual -Wformat=2 #-Wnull-dereference -Weffc++
endif

ifeq ($(DEBUG_FLAG), debug)
    CXXFLAGS += -O0 -g -DRTDEBUG=1
else
    CXXFLAGS += -O2 -DRTDEBUG=0
endif

ifneq ($(BUILD_LIB_TYPE), low_level_api)
SRC_DIR += $(SRC_ROOT)/context
SRC_DIR += $(SRC_ROOT)/utils
SRC_DIR += $(SRC_ROOT)/printf
endif

ifeq ($(BUILD_LIB_TYPE), python_api)
    SRC_DIR += $(SRC_ROOT)/standard_api $(SRC_ROOT)/high_level_api
    CXXFLAGS += -fwrapv -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2
    INCD += -I$(PY_INCD_PATH)
else
    SRC_DIR += $(SRC_ROOT)/$(BUILD_LIB_TYPE)
endif

ifeq ($(TARGET_PLATFORM), x86-linux)
    CXXFLAGS += -DX86_LINUX=1
    SRC_DIR += $(SRC_ROOT)/device/x86-linux
    INCD += -I$(SRC_ROOT)/device/x86-linux
else
    CXXFLAGS += -DARM_LINUX=1
    SRC_DIR += $(SRC_ROOT)/device/arm-linux
    INCD += -I$(SRC_ROOT)/device/arm-linux
endif

SRCS    := $(shell find $(SRC_DIR) -name '*.cpp')
SRCDIRS := $(shell find $(SRC_DIR) -name '*.cpp' -exec dirname {} \; | uniq)
SRCDIRS := $(patsubst $(SRC_ROOT)/%, %, $(SRCDIRS))
OBJS    := $(patsubst $(SRC_ROOT)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

DIR_TARGET := $(BUILD_DIR)
TARGET := $(BUILD_DIR)/$(SO_NAME).$(MAJOR).$(MINOR)
PY_TARGET := $(BUILD_DIR)/_$(SO_NAME)
TARGET_SO_NAME := $(SO_NAME).$(MAJOR)

all: $(DIR_TARGET) $(TARGET)
py:  $(DIR_TARGET) $(PY_TARGET)

$(BUILD_DIR)/%.o: $(SRC_ROOT)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCD) -c $< -o $@

$(DIR_TARGET): build-repo
	$(MD) $(BUILD_DIR)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

$(PY_TARGET): $(OBJS) $(BUILD_DIR)/libaipudrv_wrap.o
	$(CXX) $^ $(LDFLAGS) -lstdc++ -o $@

$(BUILD_DIR)/libaipudrv_wrap.o: libaipudrv.i
	swig -c++ -python libaipudrv.i
	$(CXX) $(CXXFLAGS) $(INCD) -c libaipudrv_wrap.cxx -o $@

build-repo:
	@$(call make-repo)

clean:
	$(RM) $(BUILD) libaipudrv.py libaipudrv_wrap.cxx

.phony: all clean build-repo

define make-repo
   for dir in $(SRCDIRS); \
   do \
	mkdir -p $(BUILD_DIR)/$$dir; \
   done
endef
