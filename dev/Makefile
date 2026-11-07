# dev/Makefile - builds Aleena's TEMPORARY test driver (devc). Delete with the dev/ folder.
CXX      ?= g++
CC       ?= gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -g -I../src -I../runtime
CFLAGS   := -Wall -Wextra -g -I../runtime

devc: dev_main.cpp ../src/sema.cpp ../src/interp.cpp rt_stub.c
	$(CC) $(CFLAGS) -c rt_stub.c -o rt_stub.o
	$(CXX) $(CXXFLAGS) dev_main.cpp ../src/sema.cpp ../src/interp.cpp rt_stub.o -o devc

test: devc
	./run_tests.sh

clean:
	rm -f devc rt_stub.o
