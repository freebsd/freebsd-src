# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.8 2001/07/20 06:20:13 obrien Exp $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8
SRCS=	pnpinfo.c

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lio
.endif

.include <bsd.prog.mk>
