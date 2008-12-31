#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/arm/gen/Makefile.inc,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c alloca.S fabs.c \
	infinity.c ldexp.c makecontext.c modf.c \
	setjmp.S signalcontext.c sigsetjmp.S divsi3.S
