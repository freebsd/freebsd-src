# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmbitwriter

SRCDIR=	lib/Bitcode/Writer
SRCS=	BitcodeWriter.cpp \
	BitcodeWriterPass.cpp \
	ValueEnumerator.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	BitWriter.cpp
.endif

.include "../clang.lib.mk"
