.PHONY := clean test
#CFLAGS := -O0 -g3 -fopenmp -std=c99 -Wall -pedantic $(shell pkg-config --cflags hwloc) -fopenmp
CFLAGS := -fopenmp -std=c99 -Wall -pedantic $(shell pkg-config --cflags hwloc) -fopenmp
LDLIBS := $(shell pkg-config --libs hwloc)

mtest: mtest.c

clean:
	rm -f mtest

valgrind-test: clean mtest
	valgrind --error-exitcode=128 ./mtest -n 0

test: clean mtest
	./mtest
