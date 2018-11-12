# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmipa

SRCDIR=	lib/Analysis/IPA
SRCS=	CallGraph.cpp \
	CallGraphSCCPass.cpp \
	CallPrinter.cpp \
	GlobalsModRef.cpp \
	InlineCost.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	IPA.cpp
.endif

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
