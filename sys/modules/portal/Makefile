#	$Id: Makefile,v 1.4 1998/10/16 04:30:48 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/portal
KMOD=	portal
SRCS=	portal_vfsops.c portal_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DPORTAL

.include <bsd.kmod.mk>
