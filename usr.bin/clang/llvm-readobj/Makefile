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

CFLAGS.llvm-readobj.cpp+=	-Dllvm_readobj_main=main

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${.OBJDIR}

INCFILE=	Opts.inc
TDFILE=		${LLVM_BASE}/${SRCDIR}/Opts.td
GENOPT=		-gen-opt-parser-defs
${INCFILE}: ${TDFILE}
	${LLVM_TBLGEN} ${GENOPT} -I ${LLVM_SRCS}/include -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${TDFILE}
TGHDRS+=	${INCFILE}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LINKS+=		${BINDIR}/llvm-readobj ${BINDIR}/llvm-readelf

.if ${MK_LLVM_BINUTILS} != "no"
LINKS+=		${BINDIR}/llvm-readelf ${BINDIR}/readelf
MLINKS+=		llvm-readelf.1 readelf.1
.endif

.include "../llvm.prog.mk"
