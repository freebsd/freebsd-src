#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

SRCS+=	_setjmp.S setjmp.S sigsetjmp.S \
	fabs.S modf.S \
	frexp.c infinity.c isinf.c ldexp.c \
	makecontext.c signalcontext.c
