# $FreeBSD$

LIB=	llvmarmasmparser

SRCDIR=	lib/Target/ARM/AsmParser
INCDIR=	lib/Target/ARM
SRCS=	ARMAsmLexer.cpp \
	ARMAsmParser.cpp

TGHDRS=	ARMGenAsmMatcher \
	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
