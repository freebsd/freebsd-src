#	$FreeBSD$

PROG=	makefs
COMPAT_MTREE=	getid.c misc.c spec.c
COMPAT=	fparseln.c getmode.c pack_dev.c pwcache.c stat_flags.c \
	strsuftoll.c
SRCS=	ffs.c makefs.c walk.c \
	buf.c ffs_alloc.c ffs_balloc.c mkfs.c ufs_bmap.c \
	ffs_bswap.c ffs_subr.c ffs_tables.c \
	${COMPAT} ${COMPAT_MTREE}
MAN=	makefs.8

CFLAGS+=-DHAVE_NBTOOL_CONFIG_H=1 -D_FILE_OFFSET_BITS=64
CFLAGS+=-I.
CFLAGS+=-Icompat -Icompat/mtree
CFLAGS+=-Iffs -Isys -Isys/ufs

.PATH: compat compat/mtree ffs sys/ufs/ufs sys/ufs/ffs

WARNS?=	2

.include <bsd.prog.mk>
