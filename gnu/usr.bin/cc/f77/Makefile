# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}/f ${GCCDIR}

PROG=	f77
SRCS=	gcc.c g77spec.c version.c

DPADD+=	${LIBCC_DRV} ${LIBCC_FBSD}
LDADD+=	${LIBCC_DRV} ${LIBCC_FBSD}

f77.1: g77.1
	ln -sf ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
