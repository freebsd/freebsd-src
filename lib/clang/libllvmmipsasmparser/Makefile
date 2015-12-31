# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmipsasmparser

SRCDIR=	lib/Target/Mips/AsmParser
INCDIR=	lib/Target/Mips
SRCS=	MipsAsmParser.cpp

TGHDRS=	Attributes \
	MipsGenAsmMatcher \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
