# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmobjcarcopts

SRCDIR=	lib/Transforms/ObjCARC
SRCS=	DependencyAnalysis.cpp \
	ObjCARCAPElim.cpp \
	ObjCARCContract.cpp \
	ObjCARCExpand.cpp \
	ObjCARCOpts.cpp \
	ProvenanceAnalysis.cpp \
	ProvenanceAnalysisEvaluator.cpp \
	PtrState.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	ObjCARC.cpp
.endif

TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
