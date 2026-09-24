CXX ?= clang++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude
SANFLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer -g

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = $(BIN_DIR)/aegissh

.PHONY: all clean debug san test

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

debug: CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -g -O0 -Iinclude
debug: clean $(TARGET)

ubsan: CXXFLAGS += -fsanitize=undefined -fno-omit-frame-pointer -g
ubsan: clean $(TARGET)

asan: CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g
asan: clean $(TARGET)

san: ubsan

test: $(TARGET)
	@echo "Running all phase test suites..."
	@bash tests/test_phase1.sh
	@bash tests/test_phase2.sh
	@bash tests/test_phase3.sh
	@bash tests/test_phase4.sh
	@bash tests/test_phase5.sh

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
