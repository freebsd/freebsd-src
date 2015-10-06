# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmobjcarcopts

SRCDIR=	lib/Transforms/ObjCARC
SRCS=	ARCInstKind.cpp \
	DependencyAnalysis.cpp \
	ObjCARC.cpp \
	ObjCARCAPElim.cpp \
	ObjCARCAliasAnalysis.cpp \
	ObjCARCContract.cpp \
	ObjCARCExpand.cpp \
	ObjCARCOpts.cpp \
	ProvenanceAnalysis.cpp \
	ProvenanceAnalysisEvaluator.cpp \
	PtrState.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
