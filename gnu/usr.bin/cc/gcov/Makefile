# $FreeBSD: src/gnu/usr.bin/cc/gcov/Makefile,v 1.2 2002/05/10 08:54:48 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/doc		${GCCDIR}/../libiberty

PROG=	gcov
SRCS=	gcov.c version.c
NOMAN=	#true

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
