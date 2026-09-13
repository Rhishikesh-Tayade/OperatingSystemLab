CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra

TARGET = scheduler
SRC = scheduler.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean
