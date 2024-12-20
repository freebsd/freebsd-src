.include <src.opts.mk>

PROG_CXX=	llvm-cxxfilt

SRCDIR=		llvm/tools/llvm-cxxfilt
SRCS+=		llvm-cxxfilt-driver.cpp
SRCS+=		llvm-cxxfilt.cpp

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

.if ${MK_LLVM_CXXFILT} != "no"
LINKS=		${BINDIR}/llvm-cxxfilt ${BINDIR}/c++filt
MLINKS=		llvm-cxxfilt.1 c++filt.1
.endif

.include "../llvm.prog.mk"
