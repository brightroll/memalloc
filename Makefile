CFLAGS=-Wall -g -O0
CC=gcc

ifneq ($(shell uname -s),Linux)
$(error "This code requires Linux with Glibc")
endif

.PHONY: static shared

all: static shared

static: memalloc.o

shared: libmemalloc.so

lib%.so: %.c
	$(CC) $(CFLAGS) -shared -o $@ $^

clean:
	-rm -f memalloc.o
	-rm -f libmemalloc.so
