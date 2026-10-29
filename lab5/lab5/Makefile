CXX = g++
CXXFLAGS = -g -std=c++17
COMMON = src/common/libppm.cpp
INPUT = images/input.ppm
SERVER_IP ?= 127.0.0.1
PORT ?= 12345

# Part 1: Baseline sequential
part1: src/part1/image_sharpener.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part1 src/part1/image_sharpener.cpp $(COMMON)
	./part1 $(INPUT) output_part1.ppm

# Part 2.1: Threads + Atomic Operations
part2_1_variant1: src/part2_1/part2_1_variant1.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_1_variant1 src/part2_1/part2_1_variant1.cpp $(COMMON)
	./part2_1_variant1 $(INPUT) output_part2_1_variant1.ppm
	cp output_part2_1_variant1.ppm output_part2_2_variant1.ppm 2>/dev/null || true

part2_1_variant2: src/part2_1/part2_1_variant2.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_1_variant2 src/part2_1/part2_1_variant2.cpp $(COMMON)
	./part2_1_variant2 $(INPUT) output_part2_1_variant2.ppm
	cp output_part2_1_variant2.ppm output_part2_2_variant2.ppm 2>/dev/null || true

# Part 2.2: Multi-process + Pipes
part2_2_variant1: src/part2_2/part2_2_variant1.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part2_2_variant1 src/part2_2/part2_2_variant1.cpp $(COMMON)
	./part2_2_variant1 $(INPUT) output_part2_2_variant1.ppm

part2_2_variant2: src/part2_2/part2_2_variant2.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part2_2_variant2 src/part2_2/part2_2_variant2.cpp $(COMMON)
	./part2_2_variant2 $(INPUT) output_part2_2_variant2.ppm

# Part 2.3: Shared Memory + Semaphores
part2_3: part2_3_variant1

part2_3_variant1: src/part2_3/part2_3_variant1.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_3_variant1 src/part2_3/part2_3_variant1.cpp $(COMMON)
	./part2_3_variant1 $(INPUT) output_part2_3_variant1.ppm

part2_3_variant2: src/part2_3/part2_3_variant2.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_3_variant2 src/part2_3/part2_3_variant2.cpp $(COMMON)
	./part2_3_variant2 $(INPUT) output_part2_3_variant2.ppm

# Part 3.1: Sockets - S1+S2 on A, S3 on B
part3_1_variant1_A: src/part3_1/part3_1_variant1_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant1_A src/part3_1/part3_1_variant1_A.cpp $(COMMON)
	./part3_1_variant1_A $(INPUT) $(SERVER_IP) $(PORT)

part3_1_variant1_B: src/part3_1/part3_1_variant1_B.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant1_B src/part3_1/part3_1_variant1_B.cpp $(COMMON)
	./part3_1_variant1_B $(PORT) output_part3_1_variant1.ppm

part3_1_variant2_A: src/part3_1/part3_1_variant2_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant2_A src/part3_1/part3_1_variant2_A.cpp $(COMMON)
	./part3_1_variant2_A $(INPUT) $(SERVER_IP) $(PORT)

part3_1_variant2_B: src/part3_1/part3_1_variant2_B.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant2_B src/part3_1/part3_1_variant2_B.cpp $(COMMON)
	./part3_1_variant2_B $(PORT) output_part3_1_variant2.ppm

# Part 3.2: Sockets - S1 on A, S2+S3 on B
part3_2_variant1_A: src/part3_2/part3_2_variant1_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant1_A src/part3_2/part3_2_variant1_A.cpp $(COMMON)
	./part3_2_variant1_A $(INPUT) $(SERVER_IP) $(PORT)

part3_2_variant1_B: src/part3_2/part3_2_variant1_B.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant1_B src/part3_2/part3_2_variant1_B.cpp $(COMMON)
	./part3_2_variant1_B $(PORT) output_part3_2_variant1.ppm

part3_2_variant2_A: src/part3_2/part3_2_variant2_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant2_A src/part3_2/part3_2_variant2_A.cpp $(COMMON)
	./part3_2_variant2_A $(INPUT) $(SERVER_IP) $(PORT)

part3_2_variant2_B: src/part3_2/part3_2_variant2_B.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant2_B src/part3_2/part3_2_variant2_B.cpp $(COMMON)
	./part3_2_variant2_B $(PORT) output_part3_2_variant2.ppm

# Compile all without executing
compile_all:
	$(CXX) $(CXXFLAGS) -o part1 src/part1/image_sharpener.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_1_variant1 src/part2_1/part2_1_variant1.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_1_variant2 src/part2_1/part2_1_variant2.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part2_2_variant1 src/part2_2/part2_2_variant1.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part2_2_variant2 src/part2_2/part2_2_variant2.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_3_variant1 src/part2_3/part2_3_variant1.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -pthread -o part2_3_variant2 src/part2_3/part2_3_variant2.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant1_A src/part3_1/part3_1_variant1_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant1_B src/part3_1/part3_1_variant1_B.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant2_A src/part3_1/part3_1_variant2_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_1_variant2_B src/part3_1/part3_1_variant2_B.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant1_A src/part3_2/part3_2_variant1_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant1_B src/part3_2/part3_2_variant1_B.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant2_A src/part3_2/part3_2_variant2_A.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) -o part3_2_variant2_B src/part3_2/part3_2_variant2_B.cpp $(COMMON)

# Clean
clean:
	rm -f part1 part2_1_variant1 part2_1_variant2 \
		part2_2_variant1 part2_2_variant2 \
		part2_3_variant1 part2_3_variant2 \
		part3_1_variant1_A part3_1_variant1_B \
		part3_1_variant2_A part3_1_variant2_B \
		part3_2_variant1_A part3_2_variant1_B \
		part3_2_variant2_A part3_2_variant2_B \
		output_*.ppm

.PHONY: part1 part2_1_variant1 part2_1_variant2 part2_2_variant1 part2_2_variant2 \
	part2_3 part2_3_variant1 part2_3_variant2 \
	part3_1_variant1_A part3_1_variant1_B part3_1_variant2_A part3_1_variant2_B \
	part3_2_variant1_A part3_2_variant1_B part3_2_variant2_A part3_2_variant2_B \
	compile_all clean
