# $FreeBSD: src/sys/modules/portal/Makefile,v 1.4.2.1 1999/08/29 16:27:34 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/portal
KMOD=	portal
SRCS=	portal_vfsops.c portal_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DPORTAL

.include <bsd.kmod.mk>
