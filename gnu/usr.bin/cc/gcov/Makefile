# $FreeBSD: src/gnu/usr.bin/cc/gcov/Makefile,v 1.5.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.include "../Makefile.inc"
.include "../Makefile.ver"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c errors.c
MAN=	gcov.1

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
