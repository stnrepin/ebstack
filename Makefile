CXX ?= g++
CXXFLAGS := -std=c++20
DBGFLAGS := -g -Wall -Wextra

ifdef STD_STACK_MUTEX
	CXXFLAGS += -DSTD_STACK_MUTEX
endif

CXXOBJFLAGS := $(CXXFLAGS) -c

BIN_PATH := build
SRC_PATH := .

TARGET_NAME := ebs_test
TARGET_RELEASE := $(BIN_PATH)/release/$(TARGET_NAME)
TARGET_DEBUG := $(BIN_PATH)/debug/$(TARGET_NAME)

OBJ_REL_PATH := $(BIN_PATH)/release/obj
OBJ_DBG_PATH := $(BIN_PATH)/debug/obj

SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.cpp*)))
OBJ_RELEASE := $(addprefix $(OBJ_REL_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
OBJ_DEBUG := $(addprefix $(OBJ_DBG_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

DISTCLEAN_LIST := $(OBJ_RELEASE) \
                  $(OBJ_DEBUG)
CLEAN_LIST := $(TARGET_RELEASE) \
			  $(TARGET_DEBUG) \
			  $(DISTCLEAN_LIST)

default: makedir all

$(OBJ_REL_PATH)/%.o: $(SRC_PATH)/%.cpp*
	$(CXX) $(CXXOBJFLAGS) -O3 -o $@ $<

$(OBJ_DBG_PATH)/%.o: $(SRC_PATH)/%.cpp*
	$(CXX) $(CXXOBJFLAGS) $(DBGFLAGS) -o $@ $<

$(TARGET_RELEASE): $(OBJ_RELEASE)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_RELEASE)

$(TARGET_DEBUG): $(OBJ_DEBUG)
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) $(OBJ_DEBUG) -o $@

.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_REL_PATH) $(OBJ_DBG_PATH)

.PHONY: all
all: $(TARGET_DEBUG)

.PHONY: release
release: $(TARGET_RELEASE)

.PHONY: debug
debug: $(TARGET_DEBUG)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(DISTCLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)
