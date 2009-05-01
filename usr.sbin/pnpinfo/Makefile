# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.12.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
