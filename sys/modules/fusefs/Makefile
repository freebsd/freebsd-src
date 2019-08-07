# $FreeBSD$

.PATH: ${SRCTOP}/sys/fs/fuse

KMOD=	fusefs
SRCS=	vnode_if.h \
	fuse_node.c fuse_io.c fuse_device.c fuse_ipc.c fuse_file.c \
	fuse_vfsops.c fuse_vnops.c fuse_internal.c fuse_main.c

.include <bsd.kmod.mk>
