CC=gcc
CFLAGS=-I.
all: parallel_min_max process_memory psum

parallel_min_max: parallel_min_max.c find_min_max.c utils.c find_min_max.h utils.h
	$(CC) parallel_min_max.c find_min_max.c utils.c -o parallel_min_max

process_memory: process_memory.c
	$(CC) process_memory.c -o process_memory

psum: parallel_sum.c utils.c utils.h sum.h libsum.a
	$(CC) parallel_sum.c utils.c ./libsum.a -o psum -pthread

libsum.a: sum.o
	ar rcs libsum.a sum.o

sum.o: sum.c sum.h
	$(CC) -c sum.c -o sum.o

clean:
	rm -f parallel_min_max process_memory psum sum.o libsum.a

.PHONY: all clean
