# This is the Makefile for the examples subdirectory of readline. -*- text -*-
#
EXECUTABLES = fileman rltest
CFLAGS  = -g -I../.. -I..
LDFLAGS = -g -L..

.c.o:
	$(CC) $(CFLAGS) -c $<

all: $(EXECUTABLES)

fileman: fileman.o
	$(CC) $(LDFLAGS) -o $@ fileman.o -lreadline -ltermcap

rltest: rltest.o
	$(CC) $(LDFLAGS) -o $@ rltest.o -lreadline -ltermcap

fileman.o: fileman.c
rltest.o: rltest.c
