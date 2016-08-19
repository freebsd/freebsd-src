# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmaarch64asmprinter

SRCDIR=	lib/Target/AArch64/InstPrinter
INCDIR=	lib/Target/AArch64
SRCS=	AArch64InstPrinter.cpp

TGHDRS=	AArch64GenAsmWriter \
	AArch64GenAsmWriter1 \
	AArch64GenInstrInfo \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo \
	AArch64GenSystemOperands

.include "../clang.lib.mk"
