# $FreeBSD$

.PATH:	${SRCTOP}/sys/fs/hammer2

KMOD=	hammer2
SRCS=	vnode_if.h \
	hammer2_admin.c hammer2_chain.c hammer2_cluster.c hammer2_inode.c \
	hammer2_io.c hammer2_ioctl.c hammer2_lz4.c hammer2_ondisk.c \
	hammer2_strategy.c hammer2_subr.c hammer2_vfsops.c hammer2_vnops.c \
	hammer2_xops.c \
	zlib/hammer2_zlib_adler32.c zlib/hammer2_zlib_deflate.c \
	zlib/hammer2_zlib_inffast.c zlib/hammer2_zlib_inflate.c \
	zlib/hammer2_zlib_inftrees.c zlib/hammer2_zlib_trees.c \
	zlib/hammer2_zlib_zutil.c xxhash/xxhash.c

.include <bsd.kmod.mk>
