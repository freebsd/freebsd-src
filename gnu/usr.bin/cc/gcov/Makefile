# $FreeBSD: src/gnu/usr.bin/cc/gcov/Makefile,v 1.3 2003/02/24 22:58:19 ru Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/doc		${GCCDIR}/../libiberty

PROG=	gcov
SRCS=	gcov.c version.c

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
