# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmcjit

SRCDIR=	lib/ExecutionEngine/MCJIT
SRCS=	MCJIT.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	MCJITMemoryManager.cpp
.endif

.include "../clang.lib.mk"
