.PATH: ${.CURDIR:H:H}/fs/tarfs

KMOD=	tarfs
SRCS=	opt_tarfs.h \
	vnode_if.h \
	tarfs_io.c \
	tarfs_subr.c \
	tarfs_vnops.c \
	tarfs_vfsops.c

.if !defined(KERNBUILDDIR)
CFLAGS+=	-DZSTDIO
.ifdef TARFS_DEBUG
CFLAGS+=	-DTARFS_DEBUG
.endif
.endif

SRCS+=	opt_zstdio.h
CFLAGS+=	-I${SRCTOP}/sys/contrib/zstd/lib/freebsd

.include <bsd.kmod.mk>
