# Makefile for tests for GNU MP

# Copyright (C) 1991, 1993 Free Software Foundation, Inc.

# This file is part of the GNU MP Library.

# The GNU MP Library is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# The GNU MP Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with the GNU MP Library; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

.PATH: ${.CURDIR}/.. ${.CURDIR}/../obj ${.CURDIR}/../libgmp/obj

CC = gcc

TEST_LIBS = -lgmp
OPT = -O -g
CFLAGS = -I${.CURDIR} -I${.CURDIR}/.. -I${.CURDIR}/../obj -I${.CURDIR}/../libgmp -I${.CURDIR}/../libgmp/obj $(OPT)

TEST_SRCS = tst-mul.c tst-dm.c tst-dm_ui.c tst-mdm.c tst-mdm_ui.c tst-gcd.c \
 tst-sqrtrem.c tst-convert.c
TEST_OBJS = tst-mul.o tst-dm.o tst-dm_ui.o tst-mdm.o tst-mdm_ui.o tst-gcd.o \
 tst-sqrtrem.o tst-convert.o
TESTS = tst-mul tst-dm tst-dm_ui tst-mdm tst-mdm_ui tst-gcd \
 tst-sqrtrem tst-convert

tests: $(TESTS)
	for i in $(TESTS); do echo $$i; ./$$i; done

tst-mul: tst-mul.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)
tst-dm: tst-dm.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)
tst-dm_ui: tst-dm_ui.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)
tst-mdm: tst-mdm.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)
tst-mdm_ui: tst-mdm_ui.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)
tst-gcd: tst-gcd.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)
tst-sqrtrem: tst-sqrtrem.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)
tst-convert: tst-convert.o $(TEST_LIBS)
	$(CC) $(CFLAGS) -o $@ $@.o $(TEST_LIBS)

$(TEST_PREFIX)clean:
	rm -f $(TESTS) $(TEST_OBJS) core

tst-convert.o : tst-convert.c gmp.h urandom.h 
tst-dm.o : tst-dm.c gmp.h urandom.h 
tst-dm_ui.o : tst-dm_ui.c gmp.h urandom.h 
tst-gcd.o : tst-gcd.c gmp.h urandom.h 
tst-mdm.o : tst-mdm.c gmp.h urandom.h 
tst-mdm_ui.o : tst-mdm_ui.c gmp.h urandom.h 
tst-mul.o : tst-mul.c gmp.h gmp-impl.h gmp-mparam.h longlong.h urandom.h 
tst-sqrtrem.o : tst-sqrtrem.c gmp.h urandom.h 
