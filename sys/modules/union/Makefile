# $FreeBSD: src/sys/modules/union/Makefile,v 1.4.2.1 1999/08/29 16:27:59 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/union
KMOD=	union
SRCS=	union_subr.c union_vfsops.c union_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+=	-DUNION

.include <bsd.kmod.mk>
