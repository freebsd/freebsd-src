# $FreeBSD: src/usr.sbin/apm/Makefile,v 1.17.2.1 2005/11/28 16:01:57 nyan Exp $

PROG=	apm
MAN=	apm.8
MLINKS=	apm.8 apmconf.8
MANSUBDIR= /i386
WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
