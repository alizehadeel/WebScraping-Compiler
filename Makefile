# Makefile
# OWNER: Hareem | PHASE: 1 | STATUS: UNTESTED SKELETON (written by Aleena as a starting point - fix freely)
#   [ ] make        builds ./dslc
#   [ ] make test   runs tests/run.sh
#   [ ] make clean
CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++17 -Wall -Wextra -g -Isrc -Iruntime -Ibuild
CFLAGS   ?= -Wall -Wextra -g -Iruntime $(shell xml2-config --cflags)
LDLIBS   := $(shell xml2-config --libs)

# Phase 2: add src/codegen.cpp src/codegen_recipes.cpp
CPP_OBJ := build/ast.o build/main.o build/opt.o build/sema.o build/interp.o build/parser.o build/lexer.o
C_OBJ   := build/rt.o

all: dslc

dslc: $(CPP_OBJ) $(C_OBJ)
	$(CXX) $^ -o $@ $(LDLIBS)

build:
	mkdir -p build

build/parser.cpp build/parser.hpp: src/parser.y | build
	bison -d -o build/parser.cpp $<

build/lexer.cpp: src/lexer.l build/parser.hpp | build
	flex -o $@ $<

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: build/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/rt.o: runtime/rt.c | build
	$(CC) $(CFLAGS) -c $< -o $@

test: dslc
	bash tests/run.sh

clean:
	rm -rf build dslc

.PHONY: all test clean
