# $FreeBSD: src/gnu/usr.bin/cc/cpp/Makefile,v 1.18 2000/01/24 20:12:04 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	gcc.c cppspec.c
NOMAN=	1

DPADD+=	${LIBCC_INT} ${LIBCC_FBSD}
LDADD+=	${LIBCC_INT} ${LIBCC_FBSD}

.include <bsd.prog.mk>
