# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmpowerpcdisassembler

SRCDIR=	lib/Target/PowerPC/Disassembler
INCDIR=	lib/Target/PowerPC
SRCS=	PPCDisassembler.cpp

TGHDRS=	PPCGenDisassemblerTables \
	PPCGenInstrInfo \
	PPCGenRegisterInfo \
	PPCGenSubtargetInfo

.include "../clang.lib.mk"
