# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/cp ${GCCDIR}

PROG=	c++
SRCS=	gcc.c g++spec.c
LINKS=	${BINDIR}/c++ ${BINDIR}/g++
LINKS+=	${BINDIR}/c++ ${BINDIR}/CC
NOMAN=	1

DPADD+=	${LIBCC_DRV} ${LIBCC_FBSD}
LDADD+=	${LIBCC_DRV} ${LIBCC_FBSD}

.include <bsd.prog.mk>
