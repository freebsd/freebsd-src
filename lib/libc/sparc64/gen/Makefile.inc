# $FreeBSD: src/lib/libc/sparc64/gen/Makefile.inc,v 1.12.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c \
	infinity.c ldexp.c makecontext.c modf.S \
	signalcontext.c setjmp.S sigsetjmp.S _set_tp.c
