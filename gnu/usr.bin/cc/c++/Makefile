# $FreeBSD: src/gnu/usr.bin/cc/c++/Makefile,v 1.21 2004/07/28 05:27:19 kan Exp $

.include "../Makefile.inc"
.include "../Makefile.fe"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	c++
SRCS=	gcc.c g++spec.c
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
LINKS+=	${BINDIR}/c++ ${BINDIR}/CC
NOMAN=	1

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
