# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmvectorize

SRCDIR=	lib/Transforms/Vectorize
SRCS=	BBVectorize.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	Vectorize.cpp
.endif

TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
