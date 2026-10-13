CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I../include

SRC_DIR = .
OBJ_DIR = ../obj
BIN_DIR = ..

SRCS = main.cpp trace_parser.cpp page_table_single.cpp page_table_two_level.cpp \
       tlb.cpp frame_allocator.cpp hash_util.cpp stats.cpp

OBJS   = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS   = $(OBJS:.o=.d)
TARGET = $(BIN_DIR)/simulator

ifeq ($(OS),Windows_NT)
  MKDIR = cmd /c if not exist
  RMRF  = cmd /c rmdir /s /q
  EXE   = .exe
else
  MKDIR = mkdir -p
  RMRF  = rm -rf
  EXE   =
endif

BENCHMARKS = gcc mcf lbm x264
TRACE_DIR  = ../memory_access_traces_cpu2017

.PHONY: all clean run run-all help

all: $(TARGET)$(EXE)

$(TARGET)$(EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	$(MKDIR) $(OBJ_DIR)

-include $(DEPS)

run: $(TARGET)$(EXE)
	./$(TARGET)$(EXE) --part $(or $(PART),1) --trace $(TRACE) --pt $(or $(PT),1)

run-all: $(TARGET)$(EXE)
	@for b in $(BENCHMARKS); do \
	  t=$(TRACE_DIR)/$$b-memoryTrace.txt; \
	  [ -f "$$t" ] || t=$(TRACE_DIR)/$$b.txt; \
	  if [ ! -f "$$t" ]; then echo "skip $$b (no trace at $$t)"; continue; fi; \
	  echo "=== $$b ==="; \
	  for p in 1 2 3 4; do \
	    ./$(TARGET)$(EXE) --part $$p --trace "$$t" --pt 1 --benchmark $$b \
	      > results_$${b}_part$${p}.log; \
	  done; \
	  ./$(TARGET)$(EXE) --part 4 --trace "$$t" --pt 2 --benchmark $$b \
	    > results_$${b}_part4_twolvl.log; \
	  ./$(TARGET)$(EXE) --part 3 --trace "$$t" --pt 1 --benchmark $$b \
	    --csv hypb_$$b.csv; \
	done

help:
	@echo "make            build ../simulator"
	@echo "make clean      remove objects and binary"
	@echo "make run        PART=1 TRACE=<file> PT=1|2   single run"
	@echo "make run-all    full sweep over $(BENCHMARKS)"

clean:
	$(RMRF) $(OBJ_DIR)
	$(RMRF) $(TARGET)$(EXE)