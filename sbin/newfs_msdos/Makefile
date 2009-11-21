# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
