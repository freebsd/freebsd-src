# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-cxxfilt

SRCDIR=		llvm/tools/llvm-cxxfilt
SRCS+=		llvm-cxxfilt.cpp

CFLAGS.llvm-cxxfilt.cpp+=	-Dllvm_cxxfilt_main=main

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

.if ${MK_LLVM_CXXFILT} != "no"
LINKS=		${BINDIR}/llvm-cxxfilt ${BINDIR}/c++filt
MLINKS=		llvm-cxxfilt.1 c++filt.1
.endif

.include "../llvm.prog.mk"
