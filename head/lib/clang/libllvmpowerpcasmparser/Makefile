# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmpowerpcasmparser

SRCDIR=	lib/Target/PowerPC/AsmParser
INCDIR=	lib/Target/PowerPC
SRCS=	PPCAsmParser.cpp

TGHDRS=	PPCGenAsmMatcher \
	PPCGenInstrInfo \
	PPCGenRegisterInfo \
	PPCGenSubtargetInfo

.include "../clang.lib.mk"
