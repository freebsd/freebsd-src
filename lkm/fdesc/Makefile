#	$Id: Makefile,v 1.3 1997/02/22 12:47:49 peter Exp $

.PATH:	${.CURDIR}/../../sys/miscfs/fdesc
KMOD=	fdesc_mod
SRCS=	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
