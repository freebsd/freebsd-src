# $FreeBSD$

LIB=	llvmarmdisassembler

SRCDIR=	lib/Target/ARM/Disassembler
INCDIR=	lib/Target/ARM
SRCS=	ARMDisassemblerCore.cpp

TGHDRS=	ARMGenDecoderTables \
	ARMGenEDInfo \
	ARMGenInstrInfo \
	ARMGenInstrNames \
	ARMGenRegisterInfo.h \
	ARMGenRegisterNames

.include "../clang.lib.mk"
