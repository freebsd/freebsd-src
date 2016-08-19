# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmaarch64utils

SRCDIR=	lib/Target/AArch64/Utils
INCDIR=	lib/Target/AArch64
SRCS=	AArch64BaseInfo.cpp

TGHDRS=	AArch64GenInstrInfo \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo \
	AArch64GenSystemOperands

.include "../clang.lib.mk"
