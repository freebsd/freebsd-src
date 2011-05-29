# $FreeBSD$

LIB=	llvmarmasmparser

SRCDIR=	lib/Target/ARM/AsmParser
INCDIR=	lib/Target/ARM
SRCS=	ARMAsmLexer.cpp \
	ARMAsmParser.cpp

TGHDRS=	ARMGenAsmMatcher \
	ARMGenInstrNames \
	ARMGenRegisterInfo.h \
	ARMGenRegisterNames

.include "../clang.lib.mk"
