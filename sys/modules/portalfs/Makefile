#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/miscfs/portal
KMOD=	portal_mod
SRCS=	portal_vfsops.c portal_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DPORTAL

.include <bsd.kmod.mk>
