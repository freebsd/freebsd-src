# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/portal
KMOD=	portal
SRCS=	portal_vfsops.c portal_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DPORTAL

.include <bsd.kmod.mk>
