# $FreeBSD$

LIB=	llvmx86instprinter

SRCDIR=	lib/Target/X86/InstPrinter
INCDIR=	lib/Target/X86
SRCS=	X86ATTInstPrinter.cpp \
	X86InstComments.cpp \
	X86IntelInstPrinter.cpp

TGHDRS=	X86GenAsmWriter \
	X86GenAsmWriter1 \
	X86GenInstrNames

.include "../clang.lib.mk"
