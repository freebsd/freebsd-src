# $FreeBSD: src/lib/libc/sparc64/gen/Makefile.inc,v 1.6 2002/09/14 18:06:21 tmm Exp $

SRCS+=	_setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c fpgetround.c \
	fpgetsticky.c fpsetmask.c fpsetround.c fpsetsticky.c frexp.c \
	infinity.c isinf.c ldexp.c modf.S setjmp.S sigsetjmp.S
