# $FreeBSD: src/sbin/newfs_msdos/Makefile,v 1.6 2001/12/04 02:19:54 obrien Exp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif
WARNS=	0

.include <bsd.prog.mk>
