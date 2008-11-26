# $FreeBSD: src/lib/libc/ia64/gen/Makefile.inc,v 1.13.16.1 2008/10/02 02:57:24 kensmith Exp $

SRCS+=	__divdf3.S __divdi3.S __divsf3.S __divsi3.S __moddi3.S __modsi3.S \
	__udivdi3.S __udivsi3.S __umoddi3.S __umodsi3.S _mcount.S _set_tp.c \
	_setjmp.S fabs.S flt_rounds.c fpgetmask.c fpgetround.c fpsetmask.c \
	fpsetround.c infinity.c ldexp.c makecontext.c modf.c setjmp.S \
	signalcontext.c sigsetjmp.S

# The following may go away if function _Unwind_FindTableEntry()
# will be part of GCC.
SRCS+=	unwind.c
