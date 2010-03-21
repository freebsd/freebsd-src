#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/i386/gen/Makefile.inc,v 1.23.12.1 2010/02/10 00:26:20 kensmith Exp $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c fabs.S \
	flt_rounds.c infinity.c ldexp.c makecontext.c modf.S \
	rfork_thread.S setjmp.S signalcontext.c sigsetjmp.S
