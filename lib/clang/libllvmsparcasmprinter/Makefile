# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmsparcasmprinter

SRCDIR=	lib/Target/Sparc/InstPrinter
INCDIR=	lib/Target/Sparc
SRCS=	SparcInstPrinter.cpp

TGHDRS=	SparcGenAsmWriter \
	SparcGenInstrInfo \
	SparcGenRegisterInfo \
	SparcGenSubtargetInfo

.include "../clang.lib.mk"
