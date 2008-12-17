#	$FreeBSD$

PROG=	makefs
MAN=	makefs.8

WARNS?=	2

CFLAGS+=-DHAVE_NBTOOL_CONFIG_H=1 -D_FILE_OFFSET_BITS=64

CFLAGS+=-I.
SRCS=	ffs.c makefs.c walk.c

.PATH:	ffs
CFLAGS+=-Iffs
SRCS+=	buf.c ffs_alloc.c ffs_balloc.c mkfs.c ufs_bmap.c

.PATH:	sys/ufs/ffs
CFLAGS+=-Isys -Isys/ufs
SRCS+=	ffs_bswap.c ffs_subr.c ffs_tables.c

.PATH:	${.CURDIR}/compat/mtree
CFLAGS+=-Icompat/mtree
SRCS+=	getid.c misc.c spec.c

.PATH:	compat
CFLAGS+=-Icompat
SRCS+=	fparseln.c getmode.c pack_dev.c pwcache.c stat_flags.c strsuftoll.c

.include <bsd.prog.mk>
