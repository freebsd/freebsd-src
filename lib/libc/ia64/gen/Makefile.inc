# $FreeBSD: src/lib/libc/ia64/gen/Makefile.inc,v 1.8 2003/06/24 05:06:42 marcel Exp $

SRCS+=	__divdf3.S __divdi3.S __divsf3.S __divsi3.S __moddi3.S __modsi3.S \
	__udivdi3.S __udivsi3.S __umoddi3.S __umodsi3.S _setjmp.S fabs.S \
	fpgetmask.c fpgetround.c fpsetmask.c fpsetround.c frexp.c infinity.c \
	isinf.c ldexp.c makecontext.c modf.c setjmp.S signalcontext.c \
	sigsetjmp.S

# The following may go away if function _Unwind_FindTableEntry()
# will be part of GCC.
SRCS+=	unwind.c
