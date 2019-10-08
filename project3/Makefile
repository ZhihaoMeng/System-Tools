.SUFFUXES: .c .so

CC=gcc
CFLAGS=-g -Wall -fPIC -shared 
SOURCES=allocator.c 

all: libmyalloc.so 

libmyalloc.so:
		$(CC) $(CFLAGS) $(SOURCES) -o $@ 		

clean:
		rm -rf *.so