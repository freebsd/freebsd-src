#	$Id: Makefile,v 1.1 1994/09/16 20:24:28 wollman Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/portal
KMOD=	portal_mod
SRCS=	portal_vfsops.c portal_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPORTAL

.include <bsd.kmod.mk>
