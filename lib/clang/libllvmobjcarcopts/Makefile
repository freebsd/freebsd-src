# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmobjcarcopts

SRCDIR=	lib/Transforms/ObjCARC
SRCS=	ObjCARC.cpp \
	ObjCARCOpts.cpp \
	ObjCARCExpand.cpp \
	ObjCARCAPElim.cpp \
	ObjCARCAliasAnalysis.cpp \
	ObjCARCUtil.cpp \
	ObjCARCContract.cpp \
	DependencyAnalysis.cpp \
	ProvenanceAnalysis.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
