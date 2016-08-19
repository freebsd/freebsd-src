# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmaarch64disassembler

SRCDIR=	lib/Target/AArch64/Disassembler
INCDIR=	lib/Target/AArch64
SRCS=	AArch64ExternalSymbolizer.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	AArch64Disassembler.cpp
.endif

TGHDRS=	Attributes \
	Intrinsics \
	AArch64GenDisassemblerTables \
	AArch64GenInstrInfo \
	AArch64GenRegisterInfo \
	AArch64GenSubtargetInfo \
	AArch64GenSystemOperands

.include "../clang.lib.mk"
