# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.11 2005/01/27 14:52:47 delphij Exp $

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
