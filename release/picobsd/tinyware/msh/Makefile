# Makefile for sh

CFLAGS	= -O2 #-D_POSIX_SOURCE
LDFLAGS	= -static

OBJ	= sh1.o sh2.o sh3.o sh4.o sh5.o sh6.o

all:	sh

sh:	$(OBJ)
	cc $(LDFLAGS) -o $@ $(OBJ)

$(OBJ):	sh.h

clean:
	rm -f sh *.o *.bak core
