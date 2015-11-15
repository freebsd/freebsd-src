# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmipo

SRCDIR=	lib/Transforms/IPO
SRCS=	ArgumentPromotion.cpp \
	BarrierNoopPass.cpp \
	ConstantMerge.cpp \
	DeadArgumentElimination.cpp \
	ElimAvailExtern.cpp \
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
	LowerBitSets.cpp \
	MergeFunctions.cpp \
	PartialInlining.cpp \
	PassManagerBuilder.cpp \
	PruneEH.cpp \
	StripDeadPrototypes.cpp \
	StripSymbols.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	IPO.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
