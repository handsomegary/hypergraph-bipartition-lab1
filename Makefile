CXX ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -pedantic

.PHONY: all clean
all: Lab1

Lab1: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o Lab1

clean:
	$(RM) Lab1 Lab1.exe
