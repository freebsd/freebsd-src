# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-size

SRCDIR=		llvm/tools/llvm-size
SRCS+=		llvm-size.cpp

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

.if ${MK_LLVM_BINUTILS} != "no"
LINKS=		${BINDIR}/llvm-size ${BINDIR}/size
MLINKS=		llvm-size.1 size.1
.endif

.include "../llvm.prog.mk"
