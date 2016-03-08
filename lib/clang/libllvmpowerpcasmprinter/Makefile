# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmpowerpcasmprinter

SRCDIR=	lib/Target/PowerPC/InstPrinter
INCDIR=	lib/Target/PowerPC
SRCS=	PPCInstPrinter.cpp

TGHDRS=	PPCGenAsmWriter \
	PPCGenInstrInfo \
	PPCGenRegisterInfo \
	PPCGenSubtargetInfo

.include "../clang.lib.mk"
