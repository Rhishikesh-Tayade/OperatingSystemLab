CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

TARGET   = scheduler
SRC      = scheduler.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) scheduler.txt
