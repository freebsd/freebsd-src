# $FreeBSD: src/lib/libc/powerpc/gen/Makefile.inc,v 1.8.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

SRCS += _ctx_start.S fabs.S flt_rounds.c fpgetmask.c fpgetround.c \
	fpgetsticky.c fpsetmask.c fpsetround.c \
	infinity.c ldexp.c makecontext.c modf.c _setjmp.S \
	setjmp.S sigsetjmp.S signalcontext.c syncicache.c \
	_set_tp.c


