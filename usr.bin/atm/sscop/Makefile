# $FreeBSD: src/usr.bin/atm/sscop/Makefile,v 1.1 2003/10/29 10:30:54 harti Exp $

CONTRIB= ${.CURDIR}/../../../contrib/ngatm/sscop

.PATH:	${CONTRIB}

PROG=	sscop
SRCS=	common.c sscop_main.c
WARNS?=	6
CFLAGS+= -I${CONTRIB}

DPADD+=	${LIBISC} ${LIBNETGRAPH} ${LIBNGATM}
LDADD+= ${LIBISC} ${LIBNETGRAPH} ${LIBNGATM}

.include <bsd.prog.mk>
