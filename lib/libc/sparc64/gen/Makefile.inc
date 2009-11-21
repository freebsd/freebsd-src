# $FreeBSD: src/lib/libc/sparc64/gen/Makefile.inc,v 1.12.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c \
	infinity.c ldexp.c makecontext.c modf.S \
	signalcontext.c setjmp.S sigsetjmp.S _set_tp.c
