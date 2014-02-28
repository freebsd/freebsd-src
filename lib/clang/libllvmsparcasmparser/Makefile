# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmsparcasmparser

SRCDIR=	lib/Target/Sparc/AsmParser
INCDIR=	lib/Target/Sparc
SRCS=	SparcAsmParser.cpp

TGHDRS=	SparcGenAsmMatcher \
	SparcGenInstrInfo \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
