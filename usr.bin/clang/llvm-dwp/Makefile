PROG_CXX=	llvm-dwp
MAN=

SRCDIR=		llvm/tools/llvm-dwp
SRCS+=		llvm-dwp-driver.cpp
SRCS+=		llvm-dwp.cpp

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

.include "../llvm.prog.mk"
