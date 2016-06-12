# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmtarget

SRCDIR=	lib/Target
SRCS=	Target.cpp \
	TargetLoweringObjectFile.cpp \
	TargetMachine.cpp \
	TargetMachineC.cpp \
	TargetRecip.cpp \
	TargetSubtargetInfo.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	TargetIntrinsicInfo.cpp
.endif
	
TGHDRS=	Attributes \
	Intrinsics

.include "../clang.lib.mk"
