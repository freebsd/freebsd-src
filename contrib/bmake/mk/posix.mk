# $Id: posix.mk,v 1.2 2022/03/25 23:55:37 sjg Exp $
#
#	@(#) Copyright (c) 2022, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

# The minimal set of rules required by POSIX

.if !defined(%POSIX)
.error ${.newline}Do not inlcude this directly, put .POSIX: at start of Makefile
.endif

.if ${.MAKEFLAGS:M-r} == ""
# undo some work done by sys.mk
.SUFFIXES:
.undef ARFLAGS
.undef CC CFLAGS
.undef FC FFLAGS
.undef LDFLAGS LFLAGS
.undef RANLIBFLAGS
.undef YFLAGS
.endif

.SUFFIXES: .o .c .y .l .a .sh .f

# these can still be set via environment
AR ?= ar
ARFLAGS ?= -rv
CC ?= c99
CFLAGS ?= -O
FC ?= fort77
FFLAGS ?= -O 1
LDFLAGS ?= 
LEX ?= lex
LFLAGS ?=
RANLIBFLAGS ?= -D
YACC ?= yacc
YFLAGS ?= 

.c:
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ $<


.f:
	${FC} ${FFLAGS} ${LDFLAGS} -o $@ $<


.sh:
	cp $< $@
	chmod a+x $@


.c.o:
	${CC} ${CFLAGS} -c $<


.f.o:
	${FC} ${FFLAGS} -c $<


.y.o:
	${YACC} ${YFLAGS} $<
	${CC} ${CFLAGS} -c y.tab.c
	rm -f y.tab.c
	mv y.tab.o $@


.l.o:
	${LEX} ${LFLAGS} $<
	${CC} ${CFLAGS} -c lex.yy.c
	rm -f lex.yy.c
	mv lex.yy.o $@


.y.c:
	${YACC} ${YFLAGS} $<
	mv y.tab.c $@


.l.c:
	${LEX} ${LFLAGS} $<
	mv lex.yy.c $@


.c.a:
	${CC} -c ${CFLAGS} $<
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o


.f.a:
	${FC} -c ${FFLAGS} $<
	${AR} ${ARFLAGS} $@ $*.o
	rm -f $*.o

