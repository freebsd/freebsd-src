# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
