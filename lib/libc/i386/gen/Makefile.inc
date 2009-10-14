#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# XXX: the ldexp.s should be ldexp.c see 
# http://llvm.org/bugs/show_bug.cgi?id=879
SRCS+=	_ctx_start.S _setjmp.S _set_tp.c fabs.S \
	flt_rounds.c infinity.c ldexp.s makecontext.c modf.S \
	rfork_thread.S setjmp.S signalcontext.c sigsetjmp.S
