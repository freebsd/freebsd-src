# $FreeBSD$

PROG_CXX=	llvm-strings

SRCDIR=		llvm/tools/llvm-strings
SRCS+=		llvm-strings.cpp

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

.include "../llvm.prog.mk"
