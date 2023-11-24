
.include <src.opts.mk>

PROG_CXX=	llvm-symbolizer
MAN=		llvm-symbolizer.1 llvm-addr2line.1

SRCDIR=		llvm/tools/llvm-symbolizer
SRCS+=		llvm-symbolizer.cpp

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

LINKS+=		${BINDIR}/llvm-symbolizer ${BINDIR}/llvm-addr2line

.if ${MK_LLVM_BINUTILS} != "no"
LINKS+=		${BINDIR}/llvm-symbolizer ${BINDIR}/addr2line
MLINKS+=	llvm-addr2line.1 addr2line.1
.endif

.include "../llvm.prog.mk"
