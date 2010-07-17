# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.11.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

.include <bsd.prog.mk>
