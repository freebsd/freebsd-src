#	$FreeBSD$

.PATH:	${.CURDIR}/../../sys/isofs/cd9660
KMOD=	cd9660_mod
SRCS=	cd9660_bmap.c cd9660_lookup.c cd9660_node.c cd9660_rrip.c \
	cd9660_util.c cd9660_vfsops.c cd9660_vnops.c
NOMAN=
VFS_LKM=
CFLAGS+= -DCD9660

.include <bsd.kmod.mk>
