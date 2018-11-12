# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmaarch64disassembler

SRCDIR=	lib/Target/AArch64/Disassembler
INCDIR=	lib/Target/AArch64
SRCS=	AArch64Disassembler.cpp \
	AArch64ExternalSymbolizer.cpp

TGHDRS=	Attributes \
	Intrinsics \
	AArch64GenDisassemblerTables \
	AArch64GenInstrInfo \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo

.include "../clang.lib.mk"
