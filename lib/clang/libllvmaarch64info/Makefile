# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmaarch64info

SRCDIR=	lib/Target/AArch64/TargetInfo
INCDIR=	lib/Target/AArch64
SRCS=	AArch64TargetInfo.cpp

TGHDRS=	AArch64GenInstrInfo \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo

.include "../clang.lib.mk"
