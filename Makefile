# Makefile - dslc
# OWNER: Hareem | PHASE: 1 | STATUS: DONE
#
#   make          build ./dslc
#   make test     run tests/run.sh
#   make clean    remove build output
#
# Every src/*.cpp is compiled, so new files are picked up automatically.

CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++17 -Wall -Wextra -g
CFLAGS   ?= -std=c11 -Wall -Wextra -g
BISON    ?= bison
FLEX     ?= flex

BUILD := build

# Runtime: the real libxml2 runtime/rt.c once it implements rt_open,
# otherwise Aleena's stub dev/rt_stub.c (no real HTML) so the rest builds.
ifneq ($(shell grep -l '^void *rt_open' runtime/rt.c 2>/dev/null),)
  RT_SRC     := runtime/rt.c
  XML_CFLAGS := $(shell xml2-config --cflags)
  XML_LIBS   := $(shell xml2-config --libs)
else
  RT_SRC     := dev/rt_stub.c
endif
RT_OBJ := $(BUILD)/rt.o

# Hand-written C++ sources (everything in src/ except generated ones)
SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS)) \
        $(BUILD)/parser.tab.o $(BUILD)/lexer.yy.o $(RT_OBJ)

# Test mode for `make test`.
MODE ?= --run

.PHONY: all test test-rt clean
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
$(BUILD)/rt.o: $(RT_SRC) runtime/rt.h | $(BUILD)
	$(CC) $(CFLAGS) $(XML_CFLAGS) -Iruntime -c -o $@ $<

$(BUILD):
	mkdir -p $@

test: dslc test-rt
	./tests/run.sh $(MODE)

# Alizeh's standalone runtime test (needs the real runtime/rt.c and libxml2)
test-rt: $(BUILD)/test_rt
	./$(BUILD)/test_rt > $(BUILD)/test_rt.log || { cat $(BUILD)/test_rt.log; exit 1; }
	@tail -1 $(BUILD)/test_rt.log

$(BUILD)/test_rt: runtime/test_rt.c $(RT_OBJ)
	$(CC) $(CFLAGS) $(XML_CFLAGS) -Iruntime -o $@ $^ $(XML_LIBS)

clean:
	rm -rf $(BUILD) dslc
