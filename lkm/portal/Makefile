#	$Id: Makefile,v 1.3 1997/02/22 12:48:47 peter Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/portal
KMOD=	portal_mod
SRCS=	portal_vfsops.c portal_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPORTAL

.include <bsd.kmod.mk>
