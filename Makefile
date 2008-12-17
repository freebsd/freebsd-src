#	$FreeBSD$

PROG=	makefs
MAN=	makefs.8

WARNS?=	2

CFLAGS+=-DHAVE_NBTOOL_CONFIG_H=1 -D_FILE_OFFSET_BITS=64

CFLAGS+=-I.
SRCS=	ffs.c makefs.c walk.c

.PATH:	${.CURDIR}/ffs
CFLAGS+=-Iffs
SRCS+=	buf.c ffs_alloc.c ffs_balloc.c mkfs.c ufs_bmap.c

.PATH:	${.CURDIR}/sys/ufs/ffs
CFLAGS+=-Isys -Isys/ufs
SRCS+=	ffs_bswap.c ffs_subr.c ffs_tables.c

.PATH:	${.CURDIR}/../mtree
CFLAGS+=-I../mtree
SRCS+=	misc.c spec.c

.PATH:	${.CURDIR}/compat
CFLAGS+=-Icompat
SRCS+=	fparseln.c getid.c getmode.c pwcache.c strsuftoll.c

.include <bsd.prog.mk>
