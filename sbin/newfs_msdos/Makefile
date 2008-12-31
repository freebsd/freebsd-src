# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
