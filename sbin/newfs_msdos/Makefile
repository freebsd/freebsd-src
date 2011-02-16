# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
