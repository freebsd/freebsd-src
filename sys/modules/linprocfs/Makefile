# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h linprocfs_misc.c linprocfs_subr.c \
	linprocfs_vfsops.c linprocfs_vnops.c
NOMAN=
VFS_KLD=

CFLAGS+= -DLINPROCFS

test: unload install load

load:
	kldload ${KMOD}
	mount /compat/linux/proc

unload:
	-umount /compat/linux/proc
	kldunload ${KMOD}

.include <bsd.kmod.mk>
