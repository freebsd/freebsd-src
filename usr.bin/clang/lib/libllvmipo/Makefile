# $FreeBSD$

LIB=	llvmipo

SRCDIR=	lib/Transforms/IPO
SRCS=	ArgumentPromotion.cpp ConstantMerge.cpp \
	DeadArgumentElimination.cpp DeadTypeElimination.cpp \
	ExtractGV.cpp FunctionAttrs.cpp GlobalDCE.cpp GlobalOpt.cpp \
	IPConstantPropagation.cpp IPO.cpp InlineAlways.cpp \
	InlineSimple.cpp Inliner.cpp Internalize.cpp LoopExtractor.cpp \
	LowerSetJmp.cpp MergeFunctions.cpp PartialSpecialization.cpp \
	PruneEH.cpp StripDeadPrototypes.cpp StripSymbols.cpp \
	StructRetPromotion.cpp

TGHDRS=	Intrinsics

.include "../../clang.lib.mk"
