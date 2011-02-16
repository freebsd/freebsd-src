# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.12.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
