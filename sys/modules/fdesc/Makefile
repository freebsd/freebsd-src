#	$Id: Makefile,v 1.4 1998/10/16 04:30:43 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/fdesc
KMOD=	fdesc
SRCS=	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
VFS_KLD=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
