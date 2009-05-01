# $FreeBSD: src/gnu/usr.bin/cc/c++/Makefile,v 1.24.8.1 2009/04/15 03:14:26 kensmith Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}/cp

PROG=	c++
SRCS+=	g++spec.c
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
LINKS+=	${BINDIR}/c++ ${BINDIR}/CC
NO_MAN=

DPADD=	${LIBCPP} ${LIBIBERTY}
LDADD=	${LIBCPP} ${LIBIBERTY}

.include <bsd.prog.mk>
