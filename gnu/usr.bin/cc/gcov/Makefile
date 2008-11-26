# $FreeBSD: src/gnu/usr.bin/cc/gcov/Makefile,v 1.4.20.1 2008/10/02 02:57:24 kensmith Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
