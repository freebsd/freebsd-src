# This is the Makefile for the examples subdirectory of readline. -*- text -*-
#

EXECUTABLES = fileman
CFLAGS  = -g -I../..
LDFLAGS = -g -L..

fileman: fileman.o
	$(CC) $(LDFLAGS) -o fileman fileman.o -lreadline -ltermcap

fileman.o: fileman.c
  
