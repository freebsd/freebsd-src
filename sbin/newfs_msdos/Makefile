# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
