#	Makefile,v 1.1 1994/09/21 23:27:05 wollman Exp

.PATH:	${.CURDIR}/../../sys/miscfs/fdesc
KMOD=	fdesc_mod
SRCS=	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
