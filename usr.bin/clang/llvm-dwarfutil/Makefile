# $FreeBSD$

PROG_CXX=	llvm-dwarfutil

SRCDIR=		llvm/tools/llvm-dwarfutil
SRCS+=		DebugInfoLinker.cpp
SRCS+=		llvm-dwarfutil.cpp

.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${.OBJDIR}

Options.inc: ${LLVM_BASE}/${SRCDIR}/Options.td
	${LLVM_TBLGEN} ${LLVM_TBLGEN_OPTS} --gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/include -d ${.TARGET}.d -o ${.TARGET} ${.ALLSRC}
TGHDRS+=	Options.inc

${TGHDRS}: ${LLVM_TBLGEN_BIN}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LIBADD+=	z

.include "../llvm.prog.mk"
