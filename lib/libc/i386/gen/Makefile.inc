#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/i386/gen/Makefile.inc,v 1.18 2002/11/16 06:39:11 deischen Exp $

SRCS+=	_ctx_start.S _setjmp.S alloca.S fabs.S frexp.c \
	infinity.c isinf.c ldexp.c makecontext.c modf.S \
	rfork_thread.S setjmp.S signalcontext.c sigsetjmp.S
