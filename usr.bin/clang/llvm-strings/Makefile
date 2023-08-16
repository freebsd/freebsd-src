
.include <src.opts.mk>

PROG_CXX=	llvm-strings

SRCDIR=		llvm/tools/llvm-strings
SRCS+=		llvm-strings.cpp

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

.if ${MK_LLVM_BINUTILS} != "no"
LINKS+=		${BINDIR}/llvm-strings ${BINDIR}/strings
MLINKS+=	llvm-strings.1 strings.1
.endif

.include "../llvm.prog.mk"
