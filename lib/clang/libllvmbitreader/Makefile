# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmbitreader

SRCDIR=	lib/Bitcode/Reader
SRCS=	BitcodeReader.cpp \
	BitstreamReader.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	BitReader.cpp
.endif

TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
