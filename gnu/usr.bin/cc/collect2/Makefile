# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}			${GCCDIR}/../libiberty

PROG=	collect2
SRCS=	collect2.c cp-demangle.c cplus-dem.c tlink.c
NOMAN=

DPADD=	${LIBCC_INT}
LDADD=	${LIBCC_INT}

.include <bsd.prog.mk>
