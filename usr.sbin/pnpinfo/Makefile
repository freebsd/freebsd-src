# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.4.2.1 2000/05/11 11:10:36 nyan Exp $

PROG=	pnpinfo

SRCS=	pnpinfo.c
CFLAGS+=-I${.CURDIR}/../../sys
MAN8=	pnpinfo.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lio
.endif

.PATH:	${.CURDIR}/../../contrib/pnpinfo

.include <bsd.prog.mk>
