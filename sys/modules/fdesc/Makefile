#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/miscfs/fdesc
KMOD=	fdesc_mod
SRCS=	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
