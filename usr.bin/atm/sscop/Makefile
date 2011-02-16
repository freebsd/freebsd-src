# $FreeBSD: src/usr.bin/atm/sscop/Makefile,v 1.3.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

CONTRIB= ${.CURDIR}/../../../contrib/ngatm/sscop

.PATH:	${CONTRIB}

PROG=	sscop
SRCS=	common.c sscop_main.c
WARNS?=	6
CFLAGS+= -I${CONTRIB} -DUSE_LIBBEGEMOT

DPADD=	${LIBBEGEMOT} ${LIBNETGRAPH} ${LIBNGATM}
LDADD=	-lbegemot -lnetgraph -lngatm

.include <bsd.prog.mk>
