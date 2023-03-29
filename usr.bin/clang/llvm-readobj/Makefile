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

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${.OBJDIR}

Opts.inc: ${LLVM_BASE}/${SRCDIR}/Opts.td
	${LLVM_TBLGEN} ${LLVM_TBLGEN_OPTS} --gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/include -d ${.TARGET}.d -o ${.TARGET} ${.ALLSRC}
TGHDRS+=	Opts.inc

${TGHDRS}: ${LLVM_TBLGEN_BIN}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LIBADD+=	z

LINKS+=		${BINDIR}/llvm-readobj ${BINDIR}/llvm-readelf

.if ${MK_LLVM_BINUTILS} != "no"
LINKS+=		${BINDIR}/llvm-readelf ${BINDIR}/readelf
MLINKS+=		llvm-readelf.1 readelf.1
.endif

.include "../llvm.prog.mk"
