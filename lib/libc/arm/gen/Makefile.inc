#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/arm/gen/Makefile.inc,v 1.4 2004/09/23 23:12:57 cognet Exp $

SRCS+=	_ctx_start.S _setjmp.S _set_tp.c alloca.S fabs.c \
	infinity.c ldexp.c makecontext.c modf.c \
	setjmp.S signalcontext.c sigsetjmp.S divsi3.S
