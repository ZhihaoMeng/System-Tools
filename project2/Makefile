.SUFFUXES: .c .o .a

CC=gcc
CFLAGS=-Wall -c
AR=ar
AOPTS=-cvrs
SOURCES=mythreads.c 
OBJECTS = $(SOURCES:.c=.o)

%.o : %.c 
		$(CC) $(CFLAGS) $<

all: libmythreads.a 

libmythreads.a: $@ $(OBJECTS)
		$(AR) $(AOPTS) $@ $(OBJECTS)

mythreads.o: mythreads.c

clean:
		rm -rf *.a *.o