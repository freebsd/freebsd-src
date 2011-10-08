# $FreeBSD$

.PATH: ${.CURDIR}/../../fs/portalfs

KMOD=	portalfs
SRCS=	vnode_if.h \
	portal_vfsops.c portal_vnops.c \
	opt_capsicum.h

.include <bsd.kmod.mk>
