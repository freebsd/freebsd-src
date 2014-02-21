# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmx86disassembler

SRCDIR=	lib/Target/X86/Disassembler
INCDIR=	lib/Target/X86
SRCS=	X86Disassembler.cpp

.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SRCS+=	X86DisassemblerDecoder.c
.endif

TGHDRS=	X86GenDisassemblerTables \
	X86GenInstrInfo \
	X86GenRegisterInfo

.include "../clang.lib.mk"
