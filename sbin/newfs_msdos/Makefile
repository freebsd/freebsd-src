# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
