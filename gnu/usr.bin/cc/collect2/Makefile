# $FreeBSD: src/gnu/usr.bin/cc/collect2/Makefile,v 1.6 2002/05/10 08:54:47 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}			${GCCDIR}/../libiberty

PROG=	collect2
SRCS=	collect2.c cp-demangle.c cplus-dem.c tlink.c
NOMAN=

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
