# Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.

# You should have received a copy of the GNU Library General Public
# License along with the GNU C Library; see the file COPYING.LIB.  If
# not, write to the Free Software Foundation, Inc., 675 Mass Ave,
# Cambridge, MA 02139, USA.

# Makefile for standalone distribution of malloc.

# Use this on System V.
#CPPFLAGS = -DUSG

.PHONY: all
all: libmalloc.a gmalloc.o

gmalloc = malloc.c free.c cfree.c realloc.c calloc.c morecore.c memalign.c valloc.c
sources = malloc.c free.c cfree.c realloc.c calloc.c morecore.c memalign.c valloc.c mcheck.c mtrace.c mstats.c vm-limit.c ralloc.c
objects = malloc.o free.o cfree.o realloc.o calloc.o morecore.o memalign.o valloc.o mcheck.o mtrace.o mstats.o vm-limit.o ralloc.o
headers = malloc.h

libmalloc.a: $(objects)
	ar crv $@ $(objects)
	ranlib $@

$(objects): $(headers)

gmalloc.c: gmalloc-head.c $(headers) $(gmalloc) Makefile
	cat gmalloc-head.c $(headers) $(gmalloc) > $@-tmp
	mv -f $@-tmp $@
# Make it unwritable to avoid accidentally changing the file,
# since it is generated and any changes would be lost.
	chmod a-w $@

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -I. -c $< $(OUTPUT_OPTION)

.PHONY: clean realclean malloc-clean malloc-realclean
clean malloc-clean:
	-rm -f libmalloc.a *.o core
realclean malloc-realclean: clean
	-rm -f TAGS tags *~

# For inside the C library.
malloc.tar malloc.tar.Z: FORCE
	$(MAKE) -C .. $@
FORCE:
