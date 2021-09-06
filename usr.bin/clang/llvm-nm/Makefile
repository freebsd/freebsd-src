# $FreeBSD$

.include <src.opts.mk>

PROG_CXX=	llvm-nm

SRCDIR=		llvm/tools/llvm-nm
SRCS+=		llvm-nm.cpp

LIBADD+=	z

.if ${MK_LLVM_BINUTILS} != "no"
LINKS=		${BINDIR}/llvm-nm ${BINDIR}/nm
MLINKS=		llvm-nm.1 nm.1
.endif

.include "../llvm.prog.mk"
