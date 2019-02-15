# $FreeBSD$

.include <bsd.own.mk>

LIB=	llvmasmprinter

SRCDIR=	lib/CodeGen/AsmPrinter
SRCS=	ARMException.cpp \
	AsmPrinter.cpp \
	AsmPrinterDwarf.cpp \
	AsmPrinterInlineAsm.cpp \
	DIE.cpp \
	DwarfAccelTable.cpp \
	DwarfCFIException.cpp \
	DwarfCompileUnit.cpp \
	DwarfDebug.cpp \
	DwarfException.cpp \
	ErlangGCPrinter.cpp \
	OcamlGCPrinter.cpp \
	Win64Exception.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
