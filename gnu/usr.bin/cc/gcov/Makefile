# $FreeBSD: src/gnu/usr.bin/cc/gcov/Makefile,v 1.4 2004/07/28 05:27:20 kan Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
