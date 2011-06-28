# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmipo

SRCDIR=	lib/Transforms/IPO
SRCS=	ArgumentPromotion.cpp \
	ConstantMerge.cpp \
	DeadArgumentElimination.cpp \
	DeadTypeElimination.cpp \
	ExtractGV.cpp \
	FunctionAttrs.cpp \
	GlobalDCE.cpp \
	GlobalOpt.cpp \
	IPConstantPropagation.cpp \
	InlineAlways.cpp \
	InlineSimple.cpp \
	Inliner.cpp \
	Internalize.cpp \
	LoopExtractor.cpp \
	LowerSetJmp.cpp \
	MergeFunctions.cpp \
	PartialInlining.cpp \
	PruneEH.cpp \
	StripDeadPrototypes.cpp \
	StripSymbols.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	IPO.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
