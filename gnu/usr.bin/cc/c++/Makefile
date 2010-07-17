# $FreeBSD: src/gnu/usr.bin/cc/c++/Makefile,v 1.24.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
