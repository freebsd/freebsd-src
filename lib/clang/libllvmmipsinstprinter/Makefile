# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmipsinstprinter

SRCDIR=	lib/Target/Mips/InstPrinter
INCDIR=	lib/Target/Mips
SRCS=	MipsInstPrinter.cpp

TGHDRS=	MipsGenAsmWriter \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
