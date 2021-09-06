# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-ar
MAN=		llvm-ar.1 llvm-ranlib.1

SRCDIR=		llvm/tools/llvm-ar
SRCS+=		llvm-ar.cpp

LIBADD+=	z

LINKS+=		${BINDIR}/llvm-ar ${BINDIR}/llvm-ranlib

.if ${MK_LLVM_BINUTILS} != "no"
LINKS=		${BINDIR}/llvm-ar ${BINDIR}/ar ${BINDIR}/llvm-ar ${BINDIR}/ranlib
MLINKS=		llvm-ar.1 ar.1 llvm-ar.1 ranlib.1
.endif

.include "../llvm.prog.mk"
