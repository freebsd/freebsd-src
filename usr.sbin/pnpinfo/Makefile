# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.4.2.2 2001/04/25 12:10:32 ru Exp $

PROG=	pnpinfo

SRCS=	pnpinfo.c
CFLAGS+=-I${.CURDIR}/../../sys
MAN=	pnpinfo.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lio
.endif

.PATH:	${.CURDIR}/../../contrib/pnpinfo

.include <bsd.prog.mk>
