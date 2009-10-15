# $FreeBSD$

LIB=	llvmx86asmprinter

SRCDIR=	lib/Target/X86/AsmPrinter
INCDIR=	lib/Target/X86
SRCS=	X86ATTInstPrinter.cpp X86AsmPrinter.cpp \
	X86IntelInstPrinter.cpp X86MCInstLower.cpp

TGHDRS=	X86GenAsmWriter1 X86GenAsmWriter X86GenInstrInfo \
	X86GenInstrNames X86GenRegisterInfo.h X86GenRegisterNames

.include "../../clang.lib.mk"
