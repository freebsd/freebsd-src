# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmx86disassembler

SRCDIR=	lib/Target/X86/Disassembler
INCDIR=	lib/Target/X86
SRCS=	X86Disassembler.cpp

.if ${MK_CLANG_EXTRAS} != "no" || ${MK_LLDB} != "no"
SRCS+=	X86DisassemblerDecoder.cpp
.endif

TGHDRS=	X86GenDisassemblerTables \
	X86GenInstrInfo \
	X86GenRegisterInfo \
	X86GenSubtargetInfo

.include "../clang.lib.mk"
