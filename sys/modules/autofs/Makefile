# $FreeBSD$

TOPDIR=${.CURDIR}/../..

.PATH: ${TOPDIR}/fs/autofs

CFLAGS+=	-I${TOPDIR}/fs/autofs
CFLAGS+=	-Werror
KMOD=	autofs

SRCS=	vnode_if.h \
	autofs_vfsops.c autofs_util.c autofs_vnops.c

.include <bsd.kmod.mk>
