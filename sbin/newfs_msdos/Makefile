# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.2.2.1 2001/02/21 11:24:27 nyan Exp $

PROG=	newfs_msdos
MAN8=	newfs_msdos.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
