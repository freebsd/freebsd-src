#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

SRCS+=	_ctx_start.S _setjmp.S alloca.S fabs.S frexp.c getcontext.S \
	infinity.c isinf.c ldexp.c makecontext.c modf.S \
	rfork_thread.S setjmp.S signalcontext.c sigsetjmp.S swapcontext.c
