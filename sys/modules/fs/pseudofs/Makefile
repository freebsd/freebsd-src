# $FreeBSD$

.PATH: ${.CURDIR}/../../../fs/pseudofs

CFLAGS+=-g -DINVARIANT_SUPPORT -DINVARIANTS -DWITNESS
KMOD=	pseudofs
SRCS=	vnode_if.h \
	pseudofs.c \
	pseudofs_fileno.c \
	pseudofs_vncache.c \
	pseudofs_vnops.c
NOMAN=

.include <bsd.kmod.mk>
