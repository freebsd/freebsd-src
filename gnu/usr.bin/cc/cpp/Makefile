# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	gcc.c cppspec.c
NOMAN=	1

DPADD+=	${LIBCC_INT} ${LIBCC_FBSD}
LDADD+=	${LIBCC_INT} ${LIBCC_FBSD}

.include <bsd.prog.mk>
