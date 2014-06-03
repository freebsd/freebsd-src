# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmsparcdisassembler

SRCDIR=	lib/Target/Sparc/Disassembler
INCDIR=	lib/Target/Sparc
SRCS=	SparcDisassembler.cpp

TGHDRS=	SparcGenDisassemblerTables \
	SparcGenInstrInfo \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
