# $FreeBSD: src/sys/modules/linprocfs/Makefile,v 1.1.2.3 2000/06/06 11:53:28 des Exp $

.PATH:	${.CURDIR}/../../i386/linux/linprocfs
KMOD=	linprocfs
SRCS=	vnode_if.h linprocfs_misc.c linprocfs_subr.c \
		linprocfs_vfsops.c linprocfs_vnops.c
NOMAN=
CFLAGS+= -DLINPROCFS

.include <bsd.kmod.mk>
