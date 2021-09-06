# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-size

SRCDIR=		llvm/tools/llvm-size
SRCS+=		llvm-size.cpp

.if ${MK_LLVM_BINUTILS} != "no"
LINKS=		${BINDIR}/llvm-size ${BINDIR}/size
MLINKS=		llvm-size.1 size.1
.endif

.include "../llvm.prog.mk"
