# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	cpp
SRCS=	gcc.c cppspec.c # version.c
NOMAN=	1

DPADD+=	${LIBCC_DRV} ${LIBCC_FBSD}
LDADD+=	${LIBCC_DRV} ${LIBCC_FBSD}

.include <bsd.prog.mk>
