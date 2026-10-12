CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -I../include
SRC_DIR = .
OBJ_DIR = ../obj
BIN_DIR = ..

SRCS = main.cpp trace_parser.cpp page_table_single.cpp page_table_two_level.cpp \
       tlb.cpp frame_allocator.cpp hash_util.cpp stats.cpp

OBJS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRCS))
TARGET = $(BIN_DIR)/simulator

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
