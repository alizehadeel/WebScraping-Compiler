# Makefile — dslc (owner: Hareem)
#
#   make          build ./dslc
#   make test     run tests/run.sh
#   make clean    remove build output
#
# Other members' files (sema.cpp, interp.cpp, codegen*.cpp, runtime/rt.c) are
# picked up automatically as soon as they exist in src/ and runtime/.

CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++17 -Wall -Wextra -g
CFLAGS   ?= -std=c11 -Wall -Wextra -g
BISON    ?= bison
FLEX     ?= flex

BUILD := build

# libxml2 is only needed once the runtime exists (Alizeh).
ifneq ($(wildcard runtime/rt.c),)
  XML_CFLAGS := $(shell xml2-config --cflags)
  XML_LIBS   := $(shell xml2-config --libs)
  RT_OBJ     := $(BUILD)/rt.o
endif

# Hand-written C++ sources (everything in src/ except generated ones)
SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS)) \
        $(BUILD)/parser.tab.o $(BUILD)/lexer.yy.o $(RT_OBJ)

# Test mode for `make test`. Switch to --run at milestone M2.
MODE ?= --ast

.PHONY: all test clean
all: dslc

dslc: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(XML_LIBS)

# ---- generated front end ----
$(BUILD)/parser.tab.cpp $(BUILD)/parser.tab.hpp: src/parser.y | $(BUILD)
	$(BISON) -Wall -Wcounterexamples -Werror -d -o $(BUILD)/parser.tab.cpp $<

$(BUILD)/lexer.yy.cpp: src/lexer.l $(BUILD)/parser.tab.hpp | $(BUILD)
	$(FLEX) -o $@ $<

$(BUILD)/parser.tab.o: $(BUILD)/parser.tab.cpp
	$(CXX) $(CXXFLAGS) -Wno-unused-function -Isrc -I$(BUILD) -c -o $@ $<

$(BUILD)/lexer.yy.o: $(BUILD)/lexer.yy.cpp
	$(CXX) $(CXXFLAGS) -Wno-unused-function -Wno-sign-compare -Isrc -I$(BUILD) -c -o $@ $<

# ---- hand-written C++ ----
$(BUILD)/%.o: src/%.cpp $(wildcard src/*.h) $(BUILD)/parser.tab.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(XML_CFLAGS) -Isrc -Iruntime -I$(BUILD) -c -o $@ $<

# ---- C runtime ----
$(BUILD)/rt.o: runtime/rt.c runtime/rt.h | $(BUILD)
	$(CC) $(CFLAGS) $(XML_CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@

test: dslc
	./tests/run.sh $(MODE)

clean:
	rm -rf $(BUILD) dslc
