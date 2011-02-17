# $FreeBSD$

LIB=	llvmx86asmprinter

SRCDIR=	lib/Target/X86/AsmPrinter
INCDIR=	lib/Target/X86
SRCS=	X86ATTInstPrinter.cpp \
	X86InstComments.cpp \
	X86IntelInstPrinter.cpp

TGHDRS=	X86GenAsmWriter \
	X86GenAsmWriter1 \
	X86GenInstrNames

.include "../clang.lib.mk"
