# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/fdesc
KMOD=	fdesc
SRCS=	vnode_if.h \
	fdesc_vfsops.c fdesc_vnops.c
NOMAN=
CFLAGS+= -DFDESC

.include <bsd.kmod.mk>
