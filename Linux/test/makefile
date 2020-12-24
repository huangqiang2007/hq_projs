MD  := mkdir -p
RM  := rm -rf

BUILD := ./build
BUILD_DIR := $(BUILD)/$(TARGET_PLATFORM)/$(TEST_CASE)
DIR_TARGET := $(BUILD_DIR)
TARGET := $(BUILD_DIR)/aipu_$(TEST_CASE)
CXXFLAGS = -O0 -g -Wall -Werror -std=c++11 -I../driver/umd/src -I./src
LDFLAGS = -L$(BUILD_DIR) -laipudrv -lpthread

ifeq ($(TARGET_PLATFORM), x86-linux)
    CXXFLAGS += -DX86_LINUX=1
else
    CXXFLAGS += -DARM_LINUX=1
endif

SRC_DIR := ./src
SRCS := $(wildcard $(SRC_DIR)/$(TEST_CASE)/*.cpp) \
        $(wildcard $(SRC_DIR)/common/*.cpp)
ifeq ($(TEST_CASE), pipeline_test)
    SRCS += $(wildcard $(SRC_DIR)/common/multithread/*.cpp)
else ifeq ($(TEST_CASE), multithread_test)
    SRCS += $(wildcard $(SRC_DIR)/common/multithread/*.cpp)
else ifeq ($(TEST_CASE), multithread_share_graph_test)
    SRCS += $(wildcard $(SRC_DIR)/common/multithread/*.cpp)
else ifeq ($(TEST_CASE), multithread_non_pipeline_test)
    SRCS += $(wildcard $(SRC_DIR)/common/multithread/*.cpp)
endif
OBJS = $(patsubst %cpp, %o, $(SRCS))

all: $(DIR_TARGET) $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
$(DIR_TARGET):
	$(MD) $(BUILD_DIR)
$(TARGET): $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@
	$(RM) $(OBJS)

clean:
	$(RM) $(OBJS) $(BUILD)

.PHONY: all clean
