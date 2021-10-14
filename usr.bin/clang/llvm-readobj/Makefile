# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-readobj
MAN=	llvm-readobj.1 llvm-readelf.1

SRCDIR=		llvm/tools/llvm-readobj
SRCS+=		ARMWinEHPrinter.cpp
SRCS+=		COFFDumper.cpp
SRCS+=		COFFImportDumper.cpp
SRCS+=		ELFDumper.cpp
SRCS+=		llvm-readobj.cpp
SRCS+=		MachODumper.cpp
SRCS+=		ObjDumper.cpp
SRCS+=		WasmDumper.cpp
SRCS+=		Win64EHDumper.cpp
SRCS+=		WindowsResourceDumper.cpp
SRCS+=		XCOFFDumper.cpp

LIBADD+=	z

LINKS+=		${BINDIR}/llvm-readobj ${BINDIR}/llvm-readelf
MLINKS=		llvm-readobj.1

.if ${MK_LLVM_BINUTILS} != "no"
LINKS+=		${BINDIR}/llvm-readelf ${BINDIR}/readelf
MLINKS+=		llvm-readelf.1 readelf.1
.endif

.include "../llvm.prog.mk"
