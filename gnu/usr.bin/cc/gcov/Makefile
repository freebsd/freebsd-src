# $FreeBSD: src/gnu/usr.bin/cc/gcov/Makefile,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.include "../Makefile.inc"
.include "../Makefile.ver"

.PATH: ${GCCDIR} ${GCCDIR}/doc

PROG=	gcov
SRCS=	gcov.c version.c errors.c
MAN=	gcov.1

DPADD=	${LIBIBERTY}
LDADD=	${LIBIBERTY}

.include <bsd.prog.mk>
