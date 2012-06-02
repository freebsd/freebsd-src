# $FreeBSD: head/lib/clang/libllvmmcjit/Makefile 231057 2012-02-05 23:56:22Z dim $

.include <bsd.own.mk>

LIB=	llvmmcjit

SRCDIR=	lib/ExecutionEngine/MCJIT
SRCS=	MCJIT.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	MCJITMemoryManager.cpp
.endif

.include "../clang.lib.mk"
