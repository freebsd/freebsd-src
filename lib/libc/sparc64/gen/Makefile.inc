# $FreeBSD: src/lib/libc/sparc64/gen/Makefile.inc,v 1.8 2003/08/08 15:40:28 jake Exp $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c fpsetsticky.c \
	frexp.c infinity.c isinf.c ldexp.c makecontext.c modf.S \
	signalcontext.c setjmp.S sigsetjmp.S
