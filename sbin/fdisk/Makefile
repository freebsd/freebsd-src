# $FreeBSD: src/sbin/fdisk/Makefile,v 1.13.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	fdisk
SRCS=	fdisk.c geom_mbr_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD+=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD+=	-lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
