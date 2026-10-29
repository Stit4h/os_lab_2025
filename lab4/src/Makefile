CC = gcc
AR = ar
CPPFLAGS = -I. -I../../lab3/src
CFLAGS = -std=c11 -O2 -g -Wall -Wextra -Wpedantic
LDLIBS =

.PHONY: all clean test
all: parallel_min_max process_memory zombie psum

parallel_min_max: parallel_min_max.o find_min_max.o utils.o

process_memory: process_memory.o

zombie: zombie.o

psum: parallel_sum.o utils.o libsum.a
	$(CC) $(LDFLAGS) -o $@ parallel_sum.o utils.o -L. -lsum -pthread $(LDLIBS)

libsum.a: sum.o
	$(AR) rcs $@ $^

parallel_sum.o sum.o: CFLAGS += -pthread
parallel_sum.o sum.o: sum.h
parallel_min_max.o parallel_sum.o zombie.o: cli.h
parallel_min_max.o: ../../lab3/src/find_min_max.h ../../lab3/src/utils.h
parallel_sum.o: ../../lab3/src/utils.h

utils.o: ../../lab3/src/utils.c ../../lab3/src/utils.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

find_min_max.o: ../../lab3/src/find_min_max.c ../../lab3/src/find_min_max.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: all
	python3 test_lab4.py

clean:
	rm -f parallel_min_max process_memory zombie psum *.o *.a
