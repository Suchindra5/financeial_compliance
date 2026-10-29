CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Isrc
TARGET = dfa_engine.exe
SRC = src/main.cpp

BENCH_TARGET = run_benchmarks.exe
BENCH_SRC = benchmarks/run_benchmarks.cpp

all: $(TARGET)

$(TARGET): $(SRC) src/dfa_engine.hpp src/json.hpp
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

bench: $(BENCH_TARGET)
	./$(BENCH_TARGET)

$(BENCH_TARGET): $(BENCH_SRC) src/dfa_engine.hpp src/json.hpp
	$(CXX) $(CXXFLAGS) $(BENCH_SRC) -o $(BENCH_TARGET)

clean:
	del /f /q $(TARGET) $(BENCH_TARGET)

.PHONY: all bench clean