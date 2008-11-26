# $FreeBSD: src/lib/libc/powerpc/gen/Makefile.inc,v 1.8.16.1 2008/10/02 02:57:24 kensmith Exp $

SRCS += _ctx_start.S fabs.S flt_rounds.c fpgetmask.c fpgetround.c \
	fpgetsticky.c fpsetmask.c fpsetround.c \
	infinity.c ldexp.c makecontext.c modf.c _setjmp.S \
	setjmp.S sigsetjmp.S signalcontext.c syncicache.c \
	_set_tp.c


