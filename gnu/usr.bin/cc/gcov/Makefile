# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/doc		${GCCDIR}/../libiberty

PROG=	gcov
SRCS=	gcov.c version.c
NOMAN=	#true

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
