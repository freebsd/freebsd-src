# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.2.2.2 2001/04/25 10:58:42 ru Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
