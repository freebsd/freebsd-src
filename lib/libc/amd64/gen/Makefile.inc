#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# XXX: the ldexp.s should be ldexp.c see
# http://llvm.org/bugs/show_bug.cgi?id=879
SRCS+=	_setjmp.S _set_tp.c rfork_thread.S setjmp.S sigsetjmp.S \
	fabs.S modf.S \
	infinity.c ldexp.S makecontext.c signalcontext.c \
	flt_rounds.c fpgetmask.c fpsetmask.c fpgetprec.c fpsetprec.c \
	fpgetround.c fpsetround.c fpgetsticky.c
