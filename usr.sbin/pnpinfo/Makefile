# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.4 1999/08/28 01:18:08 peter Exp $

PROG=	pnpinfo

SRCS=	pnpinfo.c
CFLAGS+=-I${.CURDIR}/../../sys
MAN8=	pnpinfo.8

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lio
.endif

.PATH:	${.CURDIR}/../../contrib/pnpinfo

.include <bsd.prog.mk>
