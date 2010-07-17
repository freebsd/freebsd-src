# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.12.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
