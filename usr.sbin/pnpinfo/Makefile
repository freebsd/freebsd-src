# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.9 2003/04/04 17:49:16 obrien Exp $

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
DPADD=  ${LIBIO}
.endif

.include <bsd.prog.mk>
