# $FreeBSD$

.PATH: ${.CURDIR}/../../../fs/pseudofs

KMOD=	pseudofs
SRCS=	vnode_if.h \
	pseudofs.c \
	pseudofs_fileno.c \
	pseudofs_vncache.c \
	pseudofs_vnops.c

.include <bsd.kmod.mk>
