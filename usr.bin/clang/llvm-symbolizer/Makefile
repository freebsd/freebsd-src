# $FreeBSD$

PROG_CXX=	llvm-symbolizer
MAN=		llvm-symbolizer.1 llvm-addr2line.1

SRCDIR=		llvm/tools/llvm-symbolizer
SRCS+=		llvm-symbolizer.cpp

LIBADD+=	z

LINKS+=		${BINDIR}/llvm-symbolizer ${BINDIR}/llvm-addr2line

.include "../llvm.prog.mk"
