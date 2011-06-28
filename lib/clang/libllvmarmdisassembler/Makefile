# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmarmdisassembler

SRCDIR=	lib/Target/ARM/Disassembler
INCDIR=	lib/Target/ARM
SRCS=	ARMDisassemblerCore.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	ARMDisassembler.cpp
.endif

TGHDRS=	ARMGenDecoderTables \
	ARMGenEDInfo \
	ARMGenInstrInfo \
	ARMGenInstrNames \
	ARMGenRegisterInfo.h \
	ARMGenRegisterNames

.include "../clang.lib.mk"
