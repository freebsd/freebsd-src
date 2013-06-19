# $NetBSD: order,v 1.1 2012/11/09 19:08:28 sjg Exp $

# Test that .ORDER is handled correctly.
# The explicit dependency the.o: the.h will make us examine the.h
# the .ORDER will prevent us building it immediately,
# we should then examine the.c rather than stop.

all: the.o

.ORDER: the.c the.h

the.c the.h:
	@echo Making $@

.SUFFIXES: .o .c

.c.o:
	@echo Making $@ from $?

the.o: the.h
