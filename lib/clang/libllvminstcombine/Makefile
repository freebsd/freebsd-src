# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvminstcombine

SRCDIR=	lib/Transforms/InstCombine
SRCS=	InstCombineAddSub.cpp \
	InstCombineAndOrXor.cpp \
	InstCombineCalls.cpp \
	InstCombineCasts.cpp \
	InstCombineCompares.cpp \
	InstCombineLoadStoreAlloca.cpp \
	InstCombineMulDivRem.cpp \
	InstCombinePHI.cpp \
	InstCombineSelect.cpp \
	InstCombineShifts.cpp \
	InstCombineSimplifyDemanded.cpp \
	InstCombineVectorOps.cpp \
	InstructionCombining.cpp

TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
