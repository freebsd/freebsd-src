# $FreeBSD: src/usr.bin/atm/sscop/Makefile,v 1.2.2.1 2004/09/26 03:10:15 des Exp $

CONTRIB= ${.CURDIR}/../../../contrib/ngatm/sscop

.PATH:	${CONTRIB}

PROG=	sscop
SRCS=	common.c sscop_main.c
WARNS?=	6
CFLAGS+= -I${CONTRIB} -DUSE_LIBBEGEMOT

DPADD=	${LIBBEGEMOT} ${LIBNETGRAPH} ${LIBNGATM}
LDADD=	-lbegemot -lnetgraph -lngatm

.include <bsd.prog.mk>
