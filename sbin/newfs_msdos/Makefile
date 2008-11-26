# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
