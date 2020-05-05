# $FreeBSD$

PROG_CXX=	llvm-ar

SRCDIR=		llvm/tools/llvm-ar
SRCS+=		llvm-ar.cpp

LIBADD+=	z

LINKS+=		${BINDIR}/llvm-ar ${BINDIR}/llvm-ranlib

.include "../llvm.prog.mk"
