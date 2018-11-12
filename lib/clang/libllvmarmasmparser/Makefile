# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmarmasmparser

SRCDIR=	lib/Target/ARM/AsmParser
INCDIR=	lib/Target/ARM
SRCS=	ARMAsmParser.cpp

TGHDRS=	ARMGenAsmMatcher \
	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
