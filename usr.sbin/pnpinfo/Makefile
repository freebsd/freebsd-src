# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.11.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.if ${MACHINE_ARCH} == "alpha"
LDADD=	-lio
DPADD=  ${LIBIO}
.endif

.include <bsd.prog.mk>
