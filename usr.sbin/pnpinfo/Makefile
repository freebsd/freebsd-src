# $FreeBSD: src/usr.sbin/pnpinfo/Makefile,v 1.10 2004/02/05 22:44:25 ru Exp $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8
SRCS=	pnpinfo.c

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.if ${MACHINE_ARCH} == "alpha"
LDADD=	-lio
DPADD=  ${LIBIO}
.endif

.include <bsd.prog.mk>
