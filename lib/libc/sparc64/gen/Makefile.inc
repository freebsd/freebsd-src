# $FreeBSD: src/lib/libc/sparc64/gen/Makefile.inc,v 1.7 2003/04/01 23:28:50 jake Exp $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c fpsetsticky.c \
	frexp.c infinity.c isinf.c ldexp.c makecontext.c modf.S setjmp.S \
	sigsetjmp.S
