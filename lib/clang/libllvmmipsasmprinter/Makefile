# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmmipsasmprinter

SRCDIR=	lib/Target/Mips/InstPrinter
INCDIR=	lib/Target/Mips
SRCS=	MipsInstPrinter.cpp

TGHDRS=	Attributes \
	MipsGenAsmWriter \
	MipsGenInstrInfo \
	MipsGenRegisterInfo \
	MipsGenSubtargetInfo

.include "../clang.lib.mk"
