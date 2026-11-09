CC = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -g -Isrc/common
LDFLAGS = -pthread -lrt

INPUT ?= images/input.ppm
HOST  ?= 127.0.0.1
PORT  ?= 12345

BINDIR = build
COMMON_SRC = src/common/libppm.cpp
COMMON_HDR = src/common/libppm.h

.PHONY: help all clean part1 \
        part2_1_variant1 part2_1_variant2 \
        part2_2_variant1 part2_2_variant2 \
        part2_3 part2_3_variant1 part2_3_variant2 \
        part3_1_variant1_A part3_1_variant1_B \
        part3_1_variant2_A part3_1_variant2_B \
        part3_2_variant1_A part3_2_variant1_B \
        part3_2_variant2_A part3_2_variant2_B \
        check

help:
	@echo "Targets:"
	@echo "  make part1"
	@echo "  make part2_1_variant1 | part2_1_variant2"
	@echo "  make part2_2_variant1 | part2_2_variant2"
	@echo "  make part2_3 | part2_3_variant1 | part2_3_variant2"
	@echo "  make part3_1_variant1_A / _B, part3_1_variant2_A / _B"
	@echo "  make part3_2_variant1_A / _B, part3_2_variant2_A / _B"
	@echo "  make check   (part1 + part2_1_variant1 + compare)"
	@echo "  make clean"
	@echo "Vars: INPUT=images/input.ppm HOST=127.0.0.1 PORT=12345"
	@echo "Part3: run _A (server) in one terminal, then _B (client) in another."

all:
	@echo "Use individual targets (part3 A/B block). See 'make help'."

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/part1: src/part1/image_sharpener.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part1/image_sharpener.cpp $(COMMON_SRC) -o $(BINDIR)/part1 $(LDFLAGS)

part1: $(BINDIR)/part1
	./$(BINDIR)/part1 $(INPUT) output_part1.ppm

$(BINDIR)/part2_1_variant1: src/part2_1/part2_1_variant1.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part2_1/part2_1_variant1.cpp $(COMMON_SRC) -o $(BINDIR)/part2_1_variant1 $(LDFLAGS)

part2_1_variant1: $(BINDIR)/part2_1_variant1
	./$(BINDIR)/part2_1_variant1 $(INPUT) output_part2_1_variant1.ppm
	cp output_part2_1_variant1.ppm output_part2_2_variant1.ppm

$(BINDIR)/part2_1_variant2: src/part2_1/part2_1_variant2.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part2_1/part2_1_variant2.cpp $(COMMON_SRC) -o $(BINDIR)/part2_1_variant2 $(LDFLAGS)

part2_1_variant2: $(BINDIR)/part2_1_variant2
	./$(BINDIR)/part2_1_variant2 $(INPUT) output_part2_1_variant2.ppm
	cp output_part2_1_variant2.ppm output_part2_2_variant2.ppm

$(BINDIR)/part2_2_variant1: src/part2_2/part2_2_variant1.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part2_2/part2_2_variant1.cpp $(COMMON_SRC) -o $(BINDIR)/part2_2_variant1 $(LDFLAGS)

part2_2_variant1: $(BINDIR)/part2_2_variant1
	./$(BINDIR)/part2_2_variant1 $(INPUT) output_part2_2_variant1.ppm

$(BINDIR)/part2_2_variant2: src/part2_2/part2_2_variant2.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part2_2/part2_2_variant2.cpp $(COMMON_SRC) -o $(BINDIR)/part2_2_variant2 $(LDFLAGS)

part2_2_variant2: $(BINDIR)/part2_2_variant2
	./$(BINDIR)/part2_2_variant2 $(INPUT) output_part2_2_variant2.ppm

$(BINDIR)/part2_3: src/part2_3/part2_3.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part2_3/part2_3.cpp $(COMMON_SRC) -o $(BINDIR)/part2_3 $(LDFLAGS)

part2_3: $(BINDIR)/part2_3
	./$(BINDIR)/part2_3 $(INPUT) output_part2_3.ppm
	cp output_part2_3.ppm output_part2_3_variant1.ppm

$(BINDIR)/part2_3_variant1: src/part2_3/part2_3_variant1.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part2_3/part2_3_variant1.cpp $(COMMON_SRC) -o $(BINDIR)/part2_3_variant1 $(LDFLAGS)

part2_3_variant1: $(BINDIR)/part2_3_variant1
	./$(BINDIR)/part2_3_variant1 $(INPUT) output_part2_3_variant1.ppm
	cp output_part2_3_variant1.ppm output_part2_3.ppm

$(BINDIR)/part2_3_variant2: src/part2_3/part2_3_variant2.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part2_3/part2_3_variant2.cpp $(COMMON_SRC) -o $(BINDIR)/part2_3_variant2 $(LDFLAGS)

part2_3_variant2: $(BINDIR)/part2_3_variant2
	./$(BINDIR)/part2_3_variant2 $(INPUT) output_part2_3_variant2.ppm

$(BINDIR)/part3_1_variant1_A: src/part3_1/part3_1_variant1_A.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_1/part3_1_variant1_A.cpp $(COMMON_SRC) -o $(BINDIR)/part3_1_variant1_A $(LDFLAGS)

part3_1_variant1_A: $(BINDIR)/part3_1_variant1_A
	./$(BINDIR)/part3_1_variant1_A $(INPUT) $(PORT)

$(BINDIR)/part3_1_variant1_B: src/part3_1/part3_1_variant1_B.cpp | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_1/part3_1_variant1_B.cpp -o $(BINDIR)/part3_1_variant1_B $(LDFLAGS)

part3_1_variant1_B: $(BINDIR)/part3_1_variant1_B
	./$(BINDIR)/part3_1_variant1_B $(HOST) $(PORT) output_part3_1_variant1.ppm

$(BINDIR)/part3_1_variant2_A: src/part3_1/part3_1_variant2_A.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_1/part3_1_variant2_A.cpp $(COMMON_SRC) -o $(BINDIR)/part3_1_variant2_A $(LDFLAGS)

part3_1_variant2_A: $(BINDIR)/part3_1_variant2_A
	./$(BINDIR)/part3_1_variant2_A $(INPUT) $(PORT)

$(BINDIR)/part3_1_variant2_B: src/part3_1/part3_1_variant2_B.cpp | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_1/part3_1_variant2_B.cpp -o $(BINDIR)/part3_1_variant2_B $(LDFLAGS)

part3_1_variant2_B: $(BINDIR)/part3_1_variant2_B
	./$(BINDIR)/part3_1_variant2_B $(HOST) $(PORT) output_part3_1_variant2.ppm

$(BINDIR)/part3_2_variant1_A: src/part3_2/part3_2_variant1_A.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_2/part3_2_variant1_A.cpp $(COMMON_SRC) -o $(BINDIR)/part3_2_variant1_A $(LDFLAGS)

part3_2_variant1_A: $(BINDIR)/part3_2_variant1_A
	./$(BINDIR)/part3_2_variant1_A $(INPUT) $(PORT)

$(BINDIR)/part3_2_variant1_B: src/part3_2/part3_2_variant1_B.cpp | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_2/part3_2_variant1_B.cpp -o $(BINDIR)/part3_2_variant1_B $(LDFLAGS)

part3_2_variant1_B: $(BINDIR)/part3_2_variant1_B
	./$(BINDIR)/part3_2_variant1_B $(HOST) $(PORT) output_part3_2_variant1.ppm

$(BINDIR)/part3_2_variant2_A: src/part3_2/part3_2_variant2_A.cpp $(COMMON_SRC) $(COMMON_HDR) | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_2/part3_2_variant2_A.cpp $(COMMON_SRC) -o $(BINDIR)/part3_2_variant2_A $(LDFLAGS)

part3_2_variant2_A: $(BINDIR)/part3_2_variant2_A
	./$(BINDIR)/part3_2_variant2_A $(INPUT) $(PORT)

$(BINDIR)/part3_2_variant2_B: src/part3_2/part3_2_variant2_B.cpp | $(BINDIR)
	$(CC) $(CXXFLAGS) src/part3_2/part3_2_variant2_B.cpp -o $(BINDIR)/part3_2_variant2_B $(LDFLAGS)

part3_2_variant2_B: $(BINDIR)/part3_2_variant2_B
	./$(BINDIR)/part3_2_variant2_B $(HOST) $(PORT) output_part3_2_variant2.ppm

check: part1 part2_1_variant1
	cmp output_part1.ppm output_part2_1_variant1.ppm && echo "CHECK PASSED: outputs identical"

clean:
	rm -rf $(BINDIR) output_*.ppm
