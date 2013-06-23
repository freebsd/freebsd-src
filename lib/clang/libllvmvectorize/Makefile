# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmvectorize

SRCDIR=	lib/Transforms/Vectorize
SRCS=	BBVectorize.cpp \
	LoopVectorize.cpp \
	SLPVectorizer.cpp \
	VecUtils.cpp \
	Vectorize.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
