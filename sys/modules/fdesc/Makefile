# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/fdesc
KMOD=	fdesc
SRCS=	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
