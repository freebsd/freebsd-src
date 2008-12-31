# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
