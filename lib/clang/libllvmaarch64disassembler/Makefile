# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmarmdisassembler

SRCDIR=	lib/Target/AArch64/Disassembler
INCDIR=	lib/Target/AArch64
SRCS=	AArch64Disassembler.cpp

TGHDRS=	AArch64GenDisassemblerTables \
	AArch64GenInstrInfo \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo

.include "../clang.lib.mk"
