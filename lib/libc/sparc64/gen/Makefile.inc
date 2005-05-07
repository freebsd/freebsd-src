# $FreeBSD: src/lib/libc/sparc64/gen/Makefile.inc,v 1.11 2004/08/15 16:18:03 dfr Exp $

SRCS+=	_ctx_start.S _setjmp.S fabs.S fixunsdfsi.S flt_rounds.c fpgetmask.c \
	fpgetround.c fpgetsticky.c fpsetmask.c fpsetround.c fpsetsticky.c \
	infinity.c ldexp.c makecontext.c modf.S \
	signalcontext.c setjmp.S sigsetjmp.S _set_tp.c
