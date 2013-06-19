# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmarminstprinter

SRCDIR=	lib/Target/ARM/InstPrinter
INCDIR=	lib/Target/ARM
SRCS=	ARMInstPrinter.cpp

TGHDRS=	ARMGenAsmWriter \
	ARMGenInstrInfo \
	ARMGenRegisterInfo \
	ARMGenSubtargetInfo

.include "../clang.lib.mk"
