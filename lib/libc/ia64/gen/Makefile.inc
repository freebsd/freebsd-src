# $FreeBSD$

SRCS+=	__divdf3.S __divdi3.S __divsf3.S __divsi3.S __moddi3.S __modsi3.S \
	__udivdi3.S __udivsi3.S __umoddi3.S __umodsi3.S _setjmp.S fabs.S \
	fpgetmask.c fpsetmask.c frexp.c infinity.c isinf.c ldexp.c modf.c \
	setjmp.S sigsetjmp.S

# The following may go away if function _Unwind_FindTableEntry()
# will be part of GCC.
SRCS+=	unwind.c
